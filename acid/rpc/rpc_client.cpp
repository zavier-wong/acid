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

RpcClient::RpcClient()
    : m_chan(s_channel_capacity) {
}

RpcClient::~RpcClient() {
    close();
}

void RpcClient::close() {
    std::unique_lock<MutexType> lock(m_mutex);

    if (m_isClose) {
        return;
    }

    m_isHeartClose = true;
    m_isClose = true;

    for (auto& i: m_responseHandle) {
        i.second << nullptr;
    }
    m_responseHandle.clear();

    m_heartTimer.stop();
    if (m_session && m_session->isConnected()) {
        m_session->close();
    }

    {
        std::unique_lock<MutexType> pubsub(m_pubsub_mutex);
        for (auto& item: m_subs) {
            item.second << false;
        }
        m_subs.clear();
    }
    m_chan.close();
    lock.unlock();
    m_recvCloseChan >> nullptr;
}

bool RpcClient::connect(Address::ptr address){
    close();
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
    m_recvCloseChan = co::co_chan<bool>{};
    m_session = std::make_shared<RpcSession>(sock);
    m_chan = co::co_chan<Protocol::ptr>(s_channel_capacity);
    go [this] {
        // 开启 recv 协程
        handleRecv();
    };
    go [this] {
        // 开启 send 协程
        handleSend();
    };

    if (m_auto_heartbeat) {
        m_heartTimer = CycleTimer(30'000, [this] {
            SPDLOG_LOGGER_DEBUG(g_logger, "heart beat");
            if (m_isHeartClose) {
                SPDLOG_LOGGER_DEBUG(g_logger, "Server closed");
                if (!m_isClose) {
                    close();
                }
                return;
            }
            // 创建心跳包
            Protocol::ptr proto = Protocol::Create(Protocol::MsgType::HEARTBEAT_PACKET, "");
            // 向 send 协程的 Channel 发送消息
            m_chan << proto;
            m_isHeartClose = true;
        });
    }

    return true;
}

void RpcClient::setTimeout(uint64_t timeout_ms) {
    m_timeout = timeout_ms;
}

void RpcClient::handleSend() {
    using namespace std::chrono_literals;
    SPDLOG_LOGGER_TRACE(g_logger, "start handleSend");
    Protocol::ptr request;
    co::co_chan<Protocol::ptr> chan = m_chan;
    // 通过 Channel 收集调用请求，如果没有消息时 Channel 内部会挂起该协程等待消息到达
    // Channel 被关闭时会退出循环
    //while (m_chan.pop(request)) {
    while (true) {
        // 貌似 libgo 的bug，chan.pop 在 close 后无法唤醒，暂时这样解决
        bool isSuccess = chan.TimedPop(request, 3s);
        if (chan.closed()) {
            break;
        }
        if (!isSuccess || !request) {
            continue;
        }
        // 发送请求
        if (m_session->sendProtocol(request) <= 0) {
            break;
        }
    }
    SPDLOG_LOGGER_TRACE(g_logger, "stop handleSend");
}

void RpcClient::handleRecv() {
    while (true) {
        // 接收响应
        Protocol::ptr proto = m_session->recvProtocol();
        if (!proto) {
            SPDLOG_LOGGER_TRACE(g_logger, "rpc {} closed", m_session->getSocket()->toString());
            if (!m_isClose) {
                go [this] {
                    close();
                };
            }
            m_recvCloseChan << true;
            break;
        }
        m_isHeartClose = false;
        Protocol::MsgType type = proto->getMsgType();
        // 判断响应类型进行对应的处理
        switch (type) {
            case Protocol::MsgType::HEARTBEAT_PACKET:
                m_isHeartClose = false;
                break;
            case Protocol::MsgType::RPC_METHOD_RESPONSE:
                // 处理调用结果
                recvProtocol(proto);
                break;
            case Protocol::MsgType::RPC_PUBSUB_REQUEST:
                handlePublish(proto);
                break;
            case Protocol::MsgType::RPC_PUBSUB_RESPONSE:
                recvProtocol(proto);
                break;
            default:
                SPDLOG_LOGGER_DEBUG(g_logger, "protocol: {}", proto->toString());
                break;
        }
    }
}

void RpcClient::handlePublish(Protocol::ptr proto) {
    PubsubRequest res;
    Serializer s(proto->getContent());
    s >> res;
    switch (res.type) {
        case PubsubMsgType::Message:
            go [res = std::move(res), this] {
                m_listener->onMessage(res.channel, res.message);
            };
            break;
        case PubsubMsgType::PatternMessage:
            go [res = std::move(res), this] {
                m_listener->onPatternMessage(res.pattern, res.channel, res.message);
            };
            break;
        default:
            SPDLOG_LOGGER_DEBUG(g_logger, "unexpect PubsubMsgType: {}", static_cast<int>(res.type));
    }
}

void RpcClient::recvProtocol(Protocol::ptr response) {
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

std::pair<Protocol::ptr, bool> RpcClient::sendProtocol(Protocol::ptr request) {
    assert(request);
    // 开启一个 Channel 接收调用结果
    co_chan<Protocol::ptr> recvChan;
    // 本次调用的序列号
    uint32_t id = 0;
    std::map<uint32_t, co::co_chan<Protocol::ptr>>::iterator it;
    {
        std::unique_lock<co::co_mutex> lock(m_mutex);
        ++m_sequenceId;
        id = m_sequenceId;
        // 将请求序列号与接收 Channel 关联
        it = m_responseHandle.emplace(m_sequenceId, recvChan).first;
    }

    // 创建请求协议，附带上请求 id
    request->setSequenceId(id);

    // 向 send 协程的 Channel 发送消息
    m_chan << request;

    bool timeout = false;
    Protocol::ptr response;
    if (m_timeout == (uint64_t)-1) {
        recvChan >> response;
    } else {
        if (!recvChan.TimedPop(response, std::chrono::milliseconds(m_timeout))) {
            timeout = true;
        }
    }

    if (!m_isClose) {
        // 删除序列号与 Channel 的映射
        m_responseHandle.erase(it);
    }

    return {response, timeout};
}

bool RpcClient::isSubscribe() {
    std::unique_lock<MutexType> lock(m_pubsub_mutex);
    return !m_subs.empty();
}

void RpcClient::unsubscribe(const std::string& channel) {
    assert(isSubscribe());
    PubsubRequest request{.type = PubsubMsgType::Unsubscribe, .channel = channel};
    Serializer s;
    s << request;
    s.reset();
    auto [response, timeout] = sendProtocol(Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString()));
    if (!response || timeout) {
        SPDLOG_LOGGER_DEBUG(g_logger, "unsubscribe channel: {} fail", channel);
        return;
    }
    {
        std::unique_lock<MutexType> pubsub(m_pubsub_mutex);
        auto it = m_subs.find(channel);
        if (it == m_subs.end()) {
            SPDLOG_LOGGER_DEBUG(g_logger, "unsubscribe channel: {} fail, no exist", channel);
            return;
        }
        it->second << true;
        m_subs.erase(it);
    }
}

bool RpcClient::subscribe(PubsubListener::ptr listener, const std::vector<std::string>& channels) {
    assert(!isSubscribe());
    co::co_chan<bool> wait;
    bool succ = true;
    size_t size{0};
    m_listener = std::move(listener);
    {
        std::unique_lock<MutexType> pubsub(m_pubsub_mutex);
        for (auto& channel: channels) {
            if (m_subs.contains(channel)) {
                SPDLOG_LOGGER_WARN(g_logger, "ignore duplicated subscribe: {}", channel);
            } else {
                m_subs.emplace(channel, co::co_chan<bool>{1});
            }
        }
        size = m_subs.size();
        wait = co::co_chan<bool>{size};

        for (auto& sub: m_subs) {
            go [wait, sub, this] {
                PubsubRequest request{.type = PubsubMsgType::Subscribe, .channel = sub.first};
                Serializer s;
                s << request;
                s.reset();
                auto [response, timeout] = sendProtocol(Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString()));
                if (!response || timeout) {
                    wait << false;
                    return;
                }
                go [channel = sub.first, this] {
                    m_listener->onSubscribe(channel);
                };
                bool res;
                sub.second >> res;
                if (res) {
                    m_listener->onUnsubscribe(sub.first);
                }
                wait << res;
            };
        }
    }
    for (size_t i = 0; i < size; i++) {
        bool res;
        wait >> res;
        if (!res) {
            succ = false;
        }
    }

    {
        std::unique_lock<MutexType> pubsub(m_pubsub_mutex);
        m_subs.clear();
    }

    m_listener = nullptr;

    return succ;
}

void RpcClient::patternUnsubscribe(const std::string& pattern) {
    assert(isSubscribe());
    PubsubRequest request{.type = PubsubMsgType::PatternUnsubscribe, .pattern = pattern};
    Serializer s;
    s << request;
    s.reset();
    auto [response, timeout] = sendProtocol(Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString()));
    if (!response || timeout) {
        SPDLOG_LOGGER_DEBUG(g_logger, "patternUnsubscribe channel: {} fail", pattern);
        return;
    }
    {
        std::unique_lock<MutexType> pubsub(m_pubsub_mutex);
        auto it = m_subs.find(pattern);
        if (it == m_subs.end()) {
            SPDLOG_LOGGER_DEBUG(g_logger, "patternUnsubscribe channel: {} fail, no exist", pattern);
            return;
        }
        it->second << true;
        m_subs.erase(it);
    }
}

bool RpcClient::patternSubscribe(PubsubListener::ptr listener, const std::vector<std::string>& patterns) {
    assert(!isSubscribe());
    co::co_chan<bool> wait;
    bool succ = true;
    size_t size{0};
    m_listener = std::move(listener);
    {
        std::unique_lock<MutexType> pubsub(m_pubsub_mutex);
        for (auto& pattern: patterns) {
            if (m_subs.contains(pattern)) {
                SPDLOG_LOGGER_WARN(g_logger, "ignore duplicated pattern subscribe: {}", pattern);
            } else {
                m_subs.emplace(pattern, co::co_chan<bool>{1});
            }
        }
        size = m_subs.size();
        wait = co::co_chan<bool>{size};

        for (auto& sub: m_subs) {
            go [wait, sub, this] {
                PubsubRequest request{.type = PubsubMsgType::PatternSubscribe, .pattern = sub.first};
                Serializer s;
                s << request;
                s.reset();
                auto [response, timeout] = sendProtocol(Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString()));
                if (!response || timeout) {
                    wait << false;
                    return;
                }
                go [pattern = sub.first, this] {
                    m_listener->onPatternSubscribe(pattern);
                };
                bool res;
                sub.second >> res;
                if (res) {
                    m_listener->onPatternUnsubscribe(sub.first);
                }
                wait << res;
            };
        }
    }
    for (size_t i = 0; i < size; i++) {
        bool res;
        wait >> res;
        if (!res) {
            succ = false;
        }
    }

    {
        std::unique_lock<MutexType> pubsub(m_pubsub_mutex);
        m_subs.clear();
    }

    m_listener = nullptr;

    return succ;
}

bool RpcClient::publish(const std::string& channel, const std::string& message) {
    assert(!isSubscribe());
    if (isClose()) {
        return false;
    }
    PubsubRequest request{.type = PubsubMsgType::Publish, .channel = channel, .message = message};
    Serializer s;
    s << request;
    s.reset();
    auto [response, timeout] = sendProtocol(Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString()));
    if (!response || timeout) {
        return false;
    }
    return true;
}

}