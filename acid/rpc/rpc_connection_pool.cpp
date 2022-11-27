//
// Created by zavier on 2022/1/16.
//

#include "acid/common/config.h"
#include "acid/rpc/rpc_connection_pool.h"

namespace acid::rpc {
static auto g_logger = GetLogInstance();

static ConfigVar<size_t>::ptr g_channel_capacity =
        Config::Lookup<size_t>("rpc.connection_pool.channel_capacity", 1024, "rpc connection pool channel capacity");

static uint64_t s_channel_capacity = 1;

struct _RpcConnectionPoolIniter{
    _RpcConnectionPoolIniter(){
        s_channel_capacity = g_channel_capacity->getValue();
        g_channel_capacity->addListener([](const size_t& old_val, const size_t& new_val) {
            SPDLOG_LOGGER_INFO(g_logger, "rpc connection pool channel capacity changed from {} to {}", old_val, new_val);
            s_channel_capacity = new_val;
        });
    }
};

static _RpcConnectionPoolIniter s_initer;

RpcConnectionPool::RpcConnectionPool(uint64_t timeout_ms, co::Scheduler* worker)
        : m_isClose(false)
        , m_timeout(timeout_ms)
        , m_chan(s_channel_capacity)
        , m_worker(worker) {

}

RpcConnectionPool::~RpcConnectionPool() {
    close();
}

void RpcConnectionPool::close() {
    if (m_isClose) {
        return;
    }

    m_isHeartClose = true;
    m_isClose = true;
    m_chan.close();

    if (m_heartTimer) {
        m_heartTimer.stop();
    }

    m_discover_handle.clear();

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
        SPDLOG_LOGGER_ERROR(g_logger, "connect to register fail");
        m_registry = nullptr;
        return false;
    }
    m_registry = std::make_shared<RpcSession>(sock);

    SPDLOG_LOGGER_DEBUG(g_logger, "connect to register {}", m_registry->getSocket()->toString());

    go co_scheduler(m_worker) [this] {
        // 开启 recv 协程
        handleRecv();
    };

    go co_scheduler(m_worker) [this] {
        // 开启 send 协程
        handleSend();
    };

    // 服务中心心跳定时器 30s
    m_heartTimer = CycleTimer(30'000, [this] {
        SPDLOG_LOGGER_DEBUG(g_logger, "heart beat");
        if (m_isHeartClose) {
            SPDLOG_LOGGER_DEBUG(g_logger, "registry closed");
            m_heartTimer.stop();
            return;
        }
        // 创建心跳包
        Protocol::ptr proto = Protocol::Create(Protocol::MsgType::HEARTBEAT_PACKET, "");
        // 向 send 协程的 Channel 发送消息
        m_chan << proto;
        m_isHeartClose = true;
    }, m_worker);
    return true;
}

void RpcConnectionPool::handleSend() {
    Protocol::ptr request;
    // 通过 Channel 收集调用请求，如果没有消息时 Channel 内部会挂起该协程等待消息到达
    // Channel 被关闭时会退出循环
    while (m_chan.pop(request)) {
        if (!request) {
            SPDLOG_LOGGER_DEBUG(g_logger, "RpcConnectionPool::handleSend() fail");
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
            SPDLOG_LOGGER_DEBUG(g_logger, "RpcConnectionPool::handleRecv() fail");
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
                SPDLOG_LOGGER_DEBUG(g_logger, "protocol:", response->toString());
                break;
        }
    }
}

void RpcConnectionPool::handleServiceDiscover(Protocol::ptr response) {
    Serializer s(response->getContent());
    std::string service;
    s >> service;
    std::map<std::string, co::co_chan<Protocol::ptr>>::iterator it;

    std::unique_lock<co::co_mutex> lock(m_discover_mutex);
    // 查找该序列号的 Channel 是否还存在，如果不存在直接返回
    it = m_discover_handle.find(service);
    if (it == m_discover_handle.end()) {
        return;
    }
    // 通过服务名获取等待该结果的 Channel
    co_chan<Protocol::ptr> chan = it->second;
    // 对该 Channel 发送调用结果唤醒调用者
    chan << response;
}

std::vector<std::string> RpcConnectionPool::discover(const std::string& name) {
    if (!m_registry || !m_registry->isConnected()) {
        return {};
    }
    // 开启一个 Channel 接收调用结果
    co_chan<Protocol::ptr> recvChan;

    std::map<std::string, co_chan<Protocol::ptr>>::iterator it;
    {
        std::unique_lock<co::co_mutex> lock(m_discover_mutex);
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
        std::unique_lock<co::co_mutex> lock(m_discover_mutex);
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

    if (!m_subHandle.contains(RPC_SERVICE_SUBSCRIBE + name)) {
        // 向注册中心订阅服务变化的消息
        subscribe(RPC_SERVICE_SUBSCRIBE + name, [name, this](Serializer s){
            // false 为服务下线，true 为新服务节点上线
            bool isNewServer = false;
            std::string addr;
            s >> isNewServer >> addr;
            std::unique_lock<co::co_mutex> lock(m_connMutex);
            if (isNewServer) {
                // 一个新的服务提供者节点加入，将服务地址加入服务列表缓存
                SPDLOG_LOGGER_DEBUG(g_logger, "service [{}:{}] join", name, addr);
                m_serviceCache[name].push_back(addr);
            } else {
                // 已有服务提供者节点下线
                SPDLOG_LOGGER_DEBUG(g_logger, "service [{}:{}] quit", name, addr);
                // 清理缓存中断开的连接地址
                auto its = m_serviceCache.find(name);
                if (its != m_serviceCache.end()) {
                    std::erase(its->second, addr);
                }
            }
        });
    }

    return rt;
}

void RpcConnectionPool::handlePublish(Protocol::ptr proto) {
    Serializer s(proto->getContent());
    std::string key;
    s >> key;
    std::unique_lock<co::co_mutex> lock(m_sub_mtx);
    auto it = m_subHandle.find(key);
    if (it == m_subHandle.end()) return;
    it->second(s);
}


}