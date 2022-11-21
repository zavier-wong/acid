//
// Created by zavier on 2022/1/16.
//

#include "acid/common/config.h"
#include "acid/rpc/rpc.h"
#include "acid/rpc/rpc_service_registry.h"

namespace acid::rpc {
static auto g_logger = GetLogInstance();

static ConfigVar<uint64_t>::ptr g_heartbeat_timeout =
        Config::Lookup<uint64_t>("rpc.registry.heartbeat_timeout",40'000,
                                 "rpc registry heartbeat timeout (ms)");

static uint64_t s_heartbeat_timeout = 0;

struct _RpcRegistryIniter{
    _RpcRegistryIniter(){
        s_heartbeat_timeout = g_heartbeat_timeout->getValue();
        g_heartbeat_timeout->addListener([](const uint64_t& old_val, const uint64_t& new_val) {
            SPDLOG_LOGGER_INFO(g_logger, "rpc registry heartbeat timeout changed from {} to {}", old_val, new_val);
            s_heartbeat_timeout = new_val;
        });
    }
};

static _RpcRegistryIniter s_initer;

RpcServiceRegistry::RpcServiceRegistry(co::Scheduler* worker, co::Scheduler* accept_worker)
        : TcpServer(worker, accept_worker)
        , m_aliveTime(s_heartbeat_timeout) {
    setName("RpcServiceRegistry");

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
}

RpcServiceRegistry::~RpcServiceRegistry() {
    {
        std::unique_lock<co::co_mutex> lock(m_sub_mtx);
        m_stop_clean = true;
    }
    m_clean_chan >> nullptr;
}

void RpcServiceRegistry::handleClient(Socket::ptr client) {
    SPDLOG_LOGGER_WARN(g_logger, "handleClient: {}", client->toString());
    RpcSession::ptr session = std::make_shared<RpcSession>(client);
    auto on_close = [client] {
        SPDLOG_LOGGER_DEBUG(g_logger, "client: {} closed", client->toString());
        client->close();
    };
    // 开启心跳定时器
    co_timer_id heartTimer = m_timer.ExpireAt(std::chrono::milliseconds(m_aliveTime), on_close);

    Address::ptr providerAddr;
    while (true) {
        Protocol::ptr request = session->recvProtocol();
        if (!request) {
            if (providerAddr) {
                SPDLOG_LOGGER_WARN(g_logger, "{} was closed; unregister {}", client->toString(), providerAddr->toString());
                handleUnregisterService(providerAddr);
            }
            return;
        }
        // 更新定时器
        heartTimer.StopTimer();
        heartTimer = m_timer.ExpireAt(std::chrono::milliseconds(m_aliveTime), on_close);

        Protocol::ptr response;
        Protocol::MsgType type = request->getMsgType();

        switch (type) {
            case Protocol::MsgType::HEARTBEAT_PACKET:
                response = handleHeartbeatPacket(request);
                break;
            case Protocol::MsgType::RPC_PROVIDER:
                SPDLOG_LOGGER_DEBUG(g_logger, client->toString());
                providerAddr = handleProvider(request, client);
                continue;
            case Protocol::MsgType::RPC_SERVICE_REGISTER:
                response = handleRegisterService(request, providerAddr);
                break;
            case Protocol::MsgType::RPC_SERVICE_DISCOVER:
                response = handleDiscoverService(request);
                break;
            case Protocol::MsgType::RPC_SUBSCRIBE_REQUEST:
                response = handleSubscribe(request, session);
                break;
            case Protocol::MsgType::RPC_PUBLISH_RESPONSE:
                continue;
            default:
                SPDLOG_LOGGER_ERROR(g_logger, "protocol: {}", request->toString());
                continue;
        }

        session->sendProtocol(response);
    }
}

Protocol::ptr RpcServiceRegistry::handleHeartbeatPacket(Protocol::ptr p) {
    return Protocol::HeartBeat();
}

Address::ptr RpcServiceRegistry::handleProvider(Protocol::ptr p, Socket::ptr sock){
    uint32_t port = 0;
    Serializer s(p->getContent());
    s.reset();
    s >> port;
    IPv4Address::ptr address(new IPv4Address(*std::dynamic_pointer_cast<IPv4Address>(sock->getRemoteAddress())));
    address->setPort(port);
    return address;
}

Protocol::ptr RpcServiceRegistry::handleRegisterService(Protocol::ptr p, Address::ptr address) {
    std::string serviceAddress = address->toString();
    std::string serviceName = p->getContent();

    std::unique_lock<co::co_mutex> lock(m_mutex);
    auto it = m_services.emplace(serviceName, serviceAddress);
    m_iters[serviceAddress].push_back(it);
    lock.unlock();

    Result<std::string> res = Result<std::string>::Success();
    res.setVal(serviceName);

    Serializer s;
    s << res;
    s.reset();

    Protocol::ptr proto =
            Protocol::Create(Protocol::MsgType::RPC_SERVICE_REGISTER_RESPONSE, s.toString());

    // 发布服务上线消息
    std::tuple<bool, std::string> data { true, serviceAddress};
    publish(RPC_SERVICE_SUBSCRIBE + serviceName, data);

    return proto;
}

void RpcServiceRegistry::handleUnregisterService(Address::ptr address) {
    std::unique_lock<co::co_mutex> lock(m_mutex);
    auto it = m_iters.find(address->toString());
    if (it == m_iters.end()) {
        return;
    }
    auto its = it->second;
    for (auto& i: its) {
        m_services.erase(i);
        // 发布服务下线消息
        std::tuple<bool, std::string> data { false, address->toString()};
        publish(RPC_SERVICE_SUBSCRIBE + i->first, data);
    }
    m_iters.erase(address->toString());
}

Protocol::ptr RpcServiceRegistry::handleDiscoverService(Protocol::ptr p) {
    std::string serviceName = p->getContent();
    std::vector<Result<std::string>> result;
    ByteArray byteArray;

    std::unique_lock<co::co_mutex> lock(m_mutex);
    auto range = m_services.equal_range(serviceName);
    uint32_t cnt = 0;
    // 未注册服务
    if (range.first == range.second) {
        cnt++;
        Result<std::string> res;
        res.setCode(RPC_NO_METHOD);
        res.setMsg("discover service:" + serviceName);
        result.push_back(res);
    } else {
        for (auto i = range.first; i != range.second; ++i) {
            Result<std::string> res;
            std::string addr;
            res.setCode(RPC_SUCCESS);
            res.setVal(i->second);
            result.push_back(res);
        }
        cnt = result.size();
    }

    Serializer s;
    s << serviceName << cnt;
    for (uint32_t i = 0; i < cnt; ++i) {
        s << result[i];
    }
    s.reset();
    Protocol::ptr proto =
            Protocol::Create(Protocol::MsgType::RPC_SERVICE_DISCOVER_RESPONSE, s.toString());
    return proto;
}

Protocol::ptr RpcServiceRegistry::handleSubscribe(Protocol::ptr proto, RpcSession::ptr client) {
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