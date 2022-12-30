//
// Created by zavier on 2022/1/13.
//
#include <fnmatch.h>
#include "acid/common/config.h"
#include "rpc_server.h"

namespace acid::rpc {
static auto g_logger = GetLogInstance();

// 心跳超时
static ConfigVar<uint64_t>::ptr g_heartbeat_timeout =
        Config::Lookup<uint64_t>("rpc.server.heartbeat_timeout",40'000,
                                 "rpc server heartbeat timeout (ms)");
// 单个客户端的最大并发
static ConfigVar<uint32_t>::ptr g_concurrent_number =
        Config::Lookup<uint32_t>("rpc.server.concurrent_number",500,
                                 "the maximum concurrency of a single client");

static uint64_t s_heartbeat_timeout = 0;
static uint32_t s_concurrent_number = 0;

struct _RpcServerIniter{
    _RpcServerIniter(){
        s_heartbeat_timeout = g_heartbeat_timeout->getValue();
        g_heartbeat_timeout->addListener([](const uint64_t& old_val, const uint64_t& new_val){
            SPDLOG_LOGGER_INFO(g_logger, "rpc server heartbeat timeout changed from {} to {}", old_val, new_val);
            s_heartbeat_timeout = new_val;
        });
        s_concurrent_number = g_concurrent_number->getValue();
        g_concurrent_number->addListener([](const uint32_t& old_val, const uint32_t& new_val){
            SPDLOG_LOGGER_INFO(g_logger, "rpc server concurrent number changed from {} to {}", old_val, new_val);
            s_concurrent_number = new_val;
        });
    }
};

static _RpcServerIniter s_initer;

RpcServer::RpcServer()
        : m_aliveTime(s_heartbeat_timeout) {
}

RpcServer::~RpcServer() {
    stop();
}

bool RpcServer::bind(Address::ptr address) {
    m_port = std::dynamic_pointer_cast<IPv4Address>(address)->getPort();
    return TcpServer::bind(address);
}

void RpcServer::setName(const std::string &name) {
    TcpServer::setName(name);
}

bool RpcServer::bindRegistry(Address::ptr address) {
    Socket::ptr sock = Socket::CreateTCP(address);

    if (!sock) {
        return false;
    }
    if (!sock->connect(address)) {
        SPDLOG_LOGGER_WARN(g_logger, "can't connect to registry");
        m_registry = nullptr;
        return false;
    }
    m_registry = std::make_shared<RpcSession>(sock);

    Serializer s;
    s << m_port;
    s.reset();

    // 向服务中心声明为provider，注册服务端口
    Protocol::ptr proto = Protocol::Create(Protocol::MsgType::RPC_PROVIDER, s.toString());
    m_registry->sendProtocol(proto);
    return true;
}

void RpcServer::start() {
    if (!isStop()) {
        return;
    }

    if (m_registry) {
        for(auto& item: m_handlers) {
            registerService(item.first);
        }
        m_registry->getSocket()->setRecvTimeout(30'000);
        // 服务中心心跳定时器 30s
        m_heartTimer = CycleTimer(30'000, [this] {
            SPDLOG_LOGGER_DEBUG(g_logger, "heart beat");
            Protocol::ptr proto = Protocol::Create(Protocol::MsgType::HEARTBEAT_PACKET, "");
            m_registry->sendProtocol(proto);
            Protocol::ptr response = m_registry->recvProtocol();
            if (!response) {
                SPDLOG_LOGGER_WARN(g_logger, "Registry close");
                //放弃注册中心，独自提供服务
                m_heartTimer.stop();
                return;
            }
        });
    }

    TcpServer::start();
}

void RpcServer::stop() {
    if (isStop()) {
        return;
    }
    m_heartTimer.stop();
    TcpServer::stop();
}


void RpcServer::handleClient(Socket::ptr client) {
    SPDLOG_LOGGER_DEBUG(g_logger, "handleClient: {}", client->toString());
    RpcSession::ptr session = std::make_shared<RpcSession>(client);
    
    auto on_close = [client] {
        SPDLOG_LOGGER_DEBUG(g_logger, "client: {} closed", client->toString());
        client->close();
    };
    // 开启心跳定时器
    co_timer_id heartTimer = m_timer.ExpireAt(std::chrono::milliseconds(m_aliveTime), on_close);
    // 限制并发
    co::co_chan<bool> wait_queue(s_concurrent_number);
    while (true) {
        Protocol::ptr request = session->recvProtocol();
        if (!request) {
            client->close();
            break;
        }
        wait_queue << true;
        // 更新定时器
        heartTimer.StopTimer();
        heartTimer = m_timer.ExpireAt(std::chrono::milliseconds(m_aliveTime), on_close);

        go [request, client, session, wait_queue, this] {
            wait_queue >> nullptr;
            Protocol::ptr response;
            Protocol::MsgType type = request->getMsgType();
            switch (type) {
                case Protocol::MsgType::HEARTBEAT_PACKET:
                    response = handleHeartbeatPacket(request);
                    break;
                case Protocol::MsgType::RPC_METHOD_REQUEST:
                    response = handleMethodCall(request);
                    break;
                case Protocol::MsgType::RPC_PUBSUB_REQUEST:
                    response = handlePubsubRequest(request, client);
                    break;
                default:
                    SPDLOG_LOGGER_ERROR(g_logger, "protocol: {}", request->toString());
                    break;
            }

            if (response && session->isConnected()) {
                session->sendProtocol(response);
            }
        };

    }

}

Serializer RpcServer::call(const std::string &name, const std::string& arg) {
    Serializer serializer;
    if (m_handlers.find(name) == m_handlers.end()) {
        return serializer;
    }
    auto fun = m_handlers[name];
    fun(serializer, arg);
    serializer.reset();
    return serializer;
}

Protocol::ptr RpcServer::handleMethodCall(Protocol::ptr p) {
    std::string func_name;
    Serializer request(p->getContent());
    request >> func_name;
    Serializer rt = call(func_name, request.toString());
    Protocol::ptr response = Protocol::Create(Protocol::MsgType::RPC_METHOD_RESPONSE, rt.toString(), p->getSequenceId());
    return response;
}

void RpcServer::registerService(const std::string &name) {
    Protocol::ptr proto = Protocol::Create(Protocol::MsgType::RPC_SERVICE_REGISTER, name);
    m_registry->sendProtocol(proto);

    Protocol::ptr rp = m_registry->recvProtocol();
    if (!rp) {
        SPDLOG_LOGGER_WARN(g_logger, "registerService: {} fail, registry socket: {}", name, m_registry->getSocket()->toString());
        return;
    }

    Result<std::string> result;
    Serializer s(rp->getContent());
    s >> result;

    if (result.getCode() != RPC_SUCCESS) {
        SPDLOG_LOGGER_WARN(g_logger, result.toString());
    } else {
        SPDLOG_LOGGER_INFO(g_logger, result.toString());
    }
}

Protocol::ptr RpcServer::handleHeartbeatPacket(Protocol::ptr p) {
    return Protocol::HeartBeat();
}

Protocol::ptr RpcServer::handlePubsubRequest(Protocol::ptr proto, Socket::ptr client) {
    PubsubRequest request;
    Serializer s(proto->getContent());
    s >> request;
    PubsubResponse response{.type = request.type};
    switch (request.type) {
        case PubsubMsgType::Publish:
            go [request, this] {
                publish(request.channel, request.message);
            };
            break;
        case PubsubMsgType::Subscribe:
            subscribe(request.channel, client);
            response.channel = request.channel;
            break;
        case PubsubMsgType::Unsubscribe:
            unsubscribe(request.channel, client);
            response.channel = request.channel;
            break;
        case PubsubMsgType::PatternSubscribe:
            patternSubscribe(request.pattern, client);
            response.pattern = request.pattern;
            break;
        case PubsubMsgType::PatternUnsubscribe:
            patternUnsubscribe(request.pattern, client);
            response.pattern = request.pattern;
            break;
        default:
            SPDLOG_LOGGER_DEBUG(g_logger, "unexpect PubsubMsgType: {}", static_cast<int>(request.type));
            return {};
            break;
    }
    s.reset();
    s << response;
    s.reset();
    return Protocol::Create(Protocol::MsgType::RPC_PUBSUB_RESPONSE, s.toString(), proto->getSequenceId());
}

void RpcServer::publish(const std::string& channel, const std::string& message) {
    std::unique_lock<MutexType> mutex(m_pubsub_mutex);
    // 找出订阅该频道的客户端
    auto it = m_pubsub_channels.find(channel);
    if (it != m_pubsub_channels.end()) {
        auto& clients_list = it->second;
        if (clients_list.empty()) {
            // 该判断已经没有客户端订阅，删除
            m_pubsub_channels.erase(it);
        } else {
            PubsubRequest request{.type = PubsubMsgType::Message, .channel = channel, .message = message};
            Serializer s;
            s << request;
            s.reset();
            Protocol::ptr proto = Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString());
            auto iter = clients_list.begin();
            while (iter != clients_list.end()) {
                if (!(*iter)->isConnected()) {
                    iter = clients_list.erase(iter);
                    continue;
                }
                RpcSession::ptr session = std::make_shared<RpcSession>(*iter, false);
                session->sendProtocol(proto);
                ++iter;
            }
        }
    }
    PubsubRequest request{.type = PubsubMsgType::PatternMessage, .channel = channel, .message = message};
    // 找出模式订阅该频道的客户端
    auto iter = m_pubsub_patterns.begin();
    while (iter != m_pubsub_patterns.end()) {
        auto& pattern = iter->first;
        auto& client = iter->second;
        if (!client->isConnected()) {
            iter = m_pubsub_patterns.erase(iter);
            continue;
        }
        // 匹配成功
        if(!fnmatch(pattern.c_str(), channel.c_str(), 0)) {
            request.pattern = pattern;
            Serializer s;
            s << request;
            s.reset();
            Protocol::ptr proto = Protocol::Create(Protocol::MsgType::RPC_PUBSUB_REQUEST, s.toString());
            RpcSession::ptr session = std::make_shared<RpcSession>(client, false);
            session->sendProtocol(proto);
        }
        ++iter;
    }
}

void RpcServer::subscribe(const std::string &channel, Socket::ptr client) {
    std::unique_lock<MutexType> mutex(m_pubsub_mutex);
    auto& clients = m_pubsub_channels[channel];
    clients.emplace_back(std::move(client));
}

void RpcServer::unsubscribe(const std::string &channel, Socket::ptr client) {
    std::unique_lock<MutexType> mutex(m_pubsub_mutex);
    auto it = m_pubsub_channels.find(channel);
    if (it == m_pubsub_channels.end()) {
        return;
    }
    auto& clients = it->second;
    auto iter = clients.begin();
    while (iter != clients.end()) {
        if (!(*iter)->isConnected() || (*iter) == client) {
            iter = clients.erase(iter);
            continue;
        }
        ++iter;
    }
    if (clients.empty()) {
        m_pubsub_channels.erase(it);
        return;
    }
}

void RpcServer::patternSubscribe(const std::string &pattern, Socket::ptr client) {
    std::unique_lock<MutexType> mutex(m_pubsub_mutex);
    m_pubsub_patterns.emplace_back(pattern, std::move(client));
}

void RpcServer::patternUnsubscribe(const std::string &pattern, Socket::ptr client) {
    std::unique_lock<MutexType> mutex(m_pubsub_mutex);
    auto iter = m_pubsub_patterns.begin();
    while (iter != m_pubsub_patterns.end()) {
        if (!iter->second->isConnected() || (iter->first == pattern && iter->second == client)) {
            iter = m_pubsub_patterns.erase(iter);
            continue;
        }
        ++iter;
    }
}

}