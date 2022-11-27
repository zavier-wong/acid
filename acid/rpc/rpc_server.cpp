//
// Created by zavier on 2022/1/13.
//
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

RpcServer::RpcServer(co::Scheduler* worker, co::Scheduler* accept_worker)
        : TcpServer(worker, accept_worker)
        , m_aliveTime(s_heartbeat_timeout) {
}

RpcServer::~RpcServer() {
    {
        std::unique_lock<co::co_mutex> lock(m_sub_mtx);
        m_stop_clean = true;
    }
    m_clean_chan >> nullptr;
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
        }, m_worker);
    }

    // 开启协程定时清理订阅列表
    go co_scheduler(m_worker) [this] {
        while (!m_stop_clean) {
            sleep(5);
            std::unique_lock<co::co_mutex> lock(m_sub_mtx);
            for (auto it = m_subscribes.cbegin(); it != m_subscribes.cend();) {
                auto conn = it->second.lock();
                if (conn == nullptr || !conn->isConnected()) {
                    it = m_subscribes.erase(it);
                } else {
                    ++it;
                }
            }
        }
        m_clean_chan << true;
    };

    return TcpServer::start();
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
            break;
        }
        wait_queue << true;
        // 更新定时器
        heartTimer.StopTimer();
        heartTimer = m_timer.ExpireAt(std::chrono::milliseconds(m_aliveTime), on_close);
        go co_scheduler(m_worker) [request, session, wait_queue, this] {
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
                case Protocol::MsgType::RPC_SUBSCRIBE_REQUEST:
                    response = handleSubscribe(request, session);
                    break;
                case Protocol::MsgType::RPC_PUBLISH_RESPONSE:
                    return;
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
    Protocol::ptr response = Protocol::Create(
            Protocol::MsgType::RPC_METHOD_RESPONSE, rt.toString(), p->getSequenceId());
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

Protocol::ptr RpcServer::handleSubscribe(Protocol::ptr proto, RpcSession::ptr client) {
    std::unique_lock<co::co_mutex> lock(m_sub_mtx);
    std::string key;
    Serializer s(proto->getContent());
    s >> key;
    m_subscribes.emplace(key, std::weak_ptr<RpcSession>(client));
    Result<> res = Result<>::Success();
    s.reset();
    s << res;
    return Protocol::Create(Protocol::MsgType::RPC_SUBSCRIBE_RESPONSE, s.toString(), 0);
}


}