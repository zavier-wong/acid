//
// Created by zavier on 2022/1/16.
//

#include "acid/config.h"
#include "acid/log.h"
#include "acid/rpc/rpc_connection_pool.h"

namespace acid::rpc {
static Logger::ptr g_logger = ACID_LOG_NAME("system");

static ConfigVar<size_t>::ptr g_channel_capacity =
        Config::Lookup<size_t>("rpc.connection_pool.channel_capacity",1024,"rpc connection pool channel capacity");

static uint64_t s_channel_capacity = 1;

struct _RpcConnectionPoolIniter{
    _RpcConnectionPoolIniter(){
        s_channel_capacity = g_channel_capacity->getValue();
        g_channel_capacity->addListener([](const size_t& old_val, const size_t& new_val){
            ACID_LOG_INFO(g_logger) << "rpc connection pool channel capacity changed from "
                                    << old_val << " to " << new_val;
            s_channel_capacity = new_val;
        });
    }
};

static _RpcConnectionPoolIniter s_initer;

RpcConnectionPool::RpcConnectionPool(uint64_t timeout_ms)
        : m_isClose(false)
        , m_timeout(timeout_ms)
        , m_chan(s_channel_capacity){

}

RpcConnectionPool::~RpcConnectionPool() {
    close();
}

void RpcConnectionPool::close() {
    ACID_LOG_DEBUG(g_logger) << "close";

    if (m_isClose) {
        return;
    }

    m_isHeartClose = true;
    m_isClose = true;
    m_chan.close();

    if (m_heartTimer) {
        m_heartTimer->cancel();
        m_heartTimer = nullptr;
    }

    m_discover_handle.clear();

    IOManager::GetThis()->delEvent(m_registry->getSocket()->getSocket(), IOManager::READ);
    if (m_registry->isConnected()) {
        m_registry->close();
    }
    m_registry.reset();
}

bool RpcConnectionPool::connect(Address::ptr address){
    Socket::ptr sock = Socket::CreateTCP(address);
    if (!sock) {
        return false;
    }
    if (!sock->connect(address, m_timeout)) {
        ACID_LOG_ERROR(g_logger) << "connect to register fail";
        m_registry = nullptr;
        return false;
    }
    m_registry = std::make_shared<RpcSession>(sock);

    ACID_LOG_DEBUG(g_logger) << "connect to registry: " << m_registry->getSocket()->toString();

    go [this] {
        // 开启 recv 协程
        handleRecv();
    };

    go [this] {
        // 开启 send 协程
        handleSend();
    };

    // 服务中心心跳定时器 30s
    m_heartTimer = acid::IOManager::GetThis()->addTimer(30'000, [this]{
        ACID_LOG_DEBUG(g_logger) << "heart beat";
        if (m_isHeartClose) {
            ACID_LOG_DEBUG(g_logger) << "registry closed";
            //放弃服务中心
            m_heartTimer->cancel();
            m_heartTimer = nullptr;
        }
        // 创建心跳包
        Protocol::ptr proto = Protocol::Create(Protocol::MsgType::HEARTBEAT_PACKET, "");
        // 向 send 协程的 Channel 发送消息
        m_chan << proto;
        m_isHeartClose = true;
    }, true);

    return true;
}

void RpcConnectionPool::handleSend() {
    Protocol::ptr request;
    // 通过 Channel 收集调用请求，如果没有消息时 Channel 内部会挂起该协程等待消息到达
    // Channel 被关闭时会退出循环
    while (m_chan >> request) {
        if (!request) {
            ACID_LOG_WARN(g_logger) << "RpcConnectionPool::handleSend() fail";
            continue;
        }
        // 发送请求
        m_registry->sendProtocol(request);
    }
}

void RpcConnectionPool::handleRecv() {
    if (!m_registry->isConnected()) {
        return;
    }
    while (true) {
        // 接收响应
        Protocol::ptr response = m_registry->recvProtocol();
        if (!response) {
            ACID_LOG_WARN(g_logger) << "RpcConnectionPool::handleRecv() fail";
            close();
            break;
        }
        m_isHeartClose = false;
        Protocol::MsgType type = response->getMsgType();
        // 判断响应类型进行对应的处理
        switch (type) {
            case Protocol::MsgType::HEARTBEAT_PACKET:
                m_isHeartClose = false;
                break;
            case Protocol::MsgType::RPC_SERVICE_DISCOVER_RESPONSE:
                handleServiceDiscover(response);
                break;
            case Protocol::MsgType::RPC_PUBLISH_REQUEST:
                handlePublish(response);
                m_chan << Protocol::Create(Protocol::MsgType::RPC_PUBLISH_RESPONSE,"");
                break;
            case Protocol::MsgType::RPC_SUBSCRIBE_RESPONSE:
                break;
            default:
                ACID_LOG_DEBUG(g_logger) << "protocol:" << response->toString();
                break;
        }
    }
}

void RpcConnectionPool::handleServiceDiscover(Protocol::ptr response) {
    Serializer s(response->getContent());
    std::string service;
    s >> service;
    std::map<std::string, Channel<Protocol::ptr>>::iterator it;

    MutexType::Lock lock(m_discover_mutex);
    // 查找该序列号的 Channel 是否还存在，如果不存在直接返回
    it = m_discover_handle.find(service);
    if (it == m_discover_handle.end()) {
        return;
    }
    // 通过服务名获取等待该结果的 Channel
    Channel<Protocol::ptr> chan = it->second;
    // 对该 Channel 发送调用结果唤醒调用者
    chan << response;
}

std::vector<std::string> RpcConnectionPool::discover(const std::string& name) {
    if (!m_registry || !m_registry->isConnected()) {
        return {};
    }
    // 开启一个 Channel 接收调用结果
    Channel<Protocol::ptr> recvChan(1);

    std::map<std::string, Channel<Protocol::ptr>>::iterator it;
    {
        MutexType::Lock lock(m_discover_mutex);
        // 将请求序列号与接收 Channel 关联
        it = m_discover_handle.emplace(name, recvChan).first;
    }

    // 创建请求协议，附带上请求 id
    Protocol::ptr request = Protocol::Create(Protocol::MsgType::RPC_SERVICE_DISCOVER, name);

    // 向 send 协程的 Channel 发送消息
    m_chan << request;

    Protocol::ptr response = nullptr;
    // 等待 response，Channel内部会挂起协程，如果有消息到达或者被关闭则会被唤醒
    recvChan >> response;

    {
        MutexType::Lock lock(m_discover_mutex);
        m_discover_handle.erase(it);
    }

    if (!response) {
        return {};
    }

    std::vector<Result<std::string>> res;
    std::vector<std::string> rt;
    std::vector<Address::ptr> addrs;

    Serializer s(response->getContent());
    uint32_t cnt;
    std::string str;
    s >> str >> cnt;

    for (uint32_t i = 0; i < cnt; ++i) {
        Result<std::string> r;
        s >> r;
        res.push_back(r);
    }

    if (res.front().getCode() == RPC_NO_METHOD) {
        return {};
    }

    for (size_t i = 0; i < res.size(); ++i) {
        rt.push_back(res[i].getVal());
    }

    return rt;
}

void RpcConnectionPool::handlePublish(Protocol::ptr proto) {
    Serializer s(proto->getContent());
    std::string key;
    s >> key;
    MutexType::Lock lock(m_sub_mtx);
    auto it = m_subHandle.find(key);
    if (it == m_subHandle.end()) return;
    it->second(s);
}


}