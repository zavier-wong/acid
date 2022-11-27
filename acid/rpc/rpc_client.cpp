//
// Created by zavier on 2022/2/16.
//
#include "acid/common/config.h"
#include "acid/rpc/rpc_client.h"

namespace acid::rpc {
static auto g_logger = GetLogInstance();

static ConfigVar<size_t>::ptr g_channel_capacity =
        Config::Lookup<size_t>("rpc.client.channel_capacity",1024,"rpc client channel capacity");

static uint64_t s_channel_capacity = 1;

struct _RpcClientIniter{
    _RpcClientIniter(){
        s_channel_capacity = g_channel_capacity->getValue();
        g_channel_capacity->addListener([](const size_t& old_val, const size_t& new_val){
            SPDLOG_LOGGER_INFO(g_logger, "rpc client channel capacity changed from {} to {}", old_val, new_val);
            s_channel_capacity = new_val;
        });
    }
};

static _RpcClientIniter s_initer;

RpcClient::RpcClient(co::Scheduler* worker)
    : m_chan(s_channel_capacity)
    , m_worker(worker) {
}

RpcClient::~RpcClient() {
    close();
}

void RpcClient::close() {
    std::unique_lock<co::co_mutex> lock(m_mutex);

    if (m_isClose) {
        return;
    }

    m_isHeartClose = true;
    m_isClose = true;
    m_chan.close();

    for (auto& i: m_responseHandle) {
        i.second << nullptr;
    }

    m_responseHandle.clear();

    if (m_heartTimer) {
        m_heartTimer.stop();
    }

    if (m_session && m_session->isConnected()) {
        m_session->close();
    }
}

bool RpcClient::connect(Address::ptr address){
    std::unique_lock<co::co_mutex> lock(m_mutex);
    Socket::ptr sock = Socket::CreateTCP(address);

    if (!sock) {
        return false;
    }
    if (!sock->connect(address, m_timeout)) {
        m_session = nullptr;
        return false;
    }
    m_isHeartClose = false;
    m_isClose = false;
    m_session = std::make_shared<RpcSession>(sock);
    m_chan = co::co_chan<Protocol::ptr>(s_channel_capacity);
    go co_scheduler(m_worker) [this] {
        // 开启 recv 协程
        handleRecv();
    };
    go co_scheduler(m_worker) [this] {
        // 开启 send 协程
        handleSend();
    };

    if (m_auto_heartbeat) {
        m_heartTimer = CycleTimer(30'000, [this] {
            SPDLOG_LOGGER_DEBUG(g_logger, "heart beat");
            if (m_isHeartClose) {
                SPDLOG_LOGGER_DEBUG(g_logger, "Server closed");
                close();
                return;
            }
            // 创建心跳包
            Protocol::ptr proto = Protocol::Create(Protocol::MsgType::HEARTBEAT_PACKET, "");
            // 向 send 协程的 Channel 发送消息
            m_chan << proto;
            m_isHeartClose = true;
        }, m_worker);
    }

    return true;
}

void RpcClient::setTimeout(uint64_t timeout_ms) {
    m_timeout = timeout_ms;
}

void RpcClient::handleSend() {
    Protocol::ptr request;
    // 通过 Channel 收集调用请求，如果没有消息时 Channel 内部会挂起该协程等待消息到达
    // Channel 被关闭时会退出循环
    while (m_chan.pop(request)) {
        if (!request) {
            SPDLOG_LOGGER_DEBUG(g_logger, "RpcClient::handleSend() fail");
            continue;
        }
        // 发送请求
        m_session->sendProtocol(request);
    }
}

void RpcClient::handleRecv() {
    if (!m_session->isConnected()) {
        return;
    }
    while (true) {
        // 接收响应
        Protocol::ptr response = m_session->recvProtocol();
        if (!response) {
            SPDLOG_LOGGER_DEBUG(g_logger, "RpcClient::handleRecv() fail");
            // close();
            break;
        }
        m_isHeartClose = false;
        Protocol::MsgType type = response->getMsgType();
        // 判断响应类型进行对应的处理
        switch (type) {
            case Protocol::MsgType::HEARTBEAT_PACKET:
                m_isHeartClose = false;
                break;
            case Protocol::MsgType::RPC_METHOD_RESPONSE:
                // 处理调用结果
                handleMethodResponse(response);
                break;
            case Protocol::MsgType::RPC_PUBLISH_REQUEST:
                handlePublish(response);
                m_chan << Protocol::Create(Protocol::MsgType::RPC_PUBLISH_RESPONSE,"");
                break;
            case Protocol::MsgType::RPC_SUBSCRIBE_RESPONSE:
                break;
            default:
                SPDLOG_LOGGER_DEBUG(g_logger, "protocol: {}", response->toString());
                break;
        }
    }
}

void RpcClient::handleMethodResponse(Protocol::ptr response) {
    // 获取该调用结果的序列号
    uint32_t id = response->getSequenceId();
    std::map<uint32_t, co::co_chan<Protocol::ptr>>::iterator it;

    std::unique_lock<co::co_mutex> lock(m_mutex);
    // 查找该序列号的 Channel 是否还存在，如果不存在直接返回
    it = m_responseHandle.find(id);
    if (it == m_responseHandle.end()) {
        return;
    }
    // 通过序列号获取等待该结果的 Channel
    co_chan<Protocol::ptr> chan = it->second;
    // 对该 Channel 发送调用结果唤醒调用者
    chan << response;
}

void RpcClient::handlePublish(Protocol::ptr proto) {
    Serializer s(proto->getContent());
    std::string key;
    s >> key;
    std::unique_lock<co::co_mutex> lock(m_sub_mtx);
    auto it = m_subHandle.find(key);
    if (it == m_subHandle.end()) return;
    it->second(s);
}


}