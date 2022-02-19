//
// Created by zavier on 2022/1/16.
//

#include "acid/log.h"
#include "acid/rpc/rpc_connection_pool.h"

namespace acid::rpc {
static Logger::ptr g_logger = ACID_LOG_NAME("system");

RpcConnectionPool::RpcConnectionPool(uint64_t timeout_ms)
        : m_timeout(timeout_ms){

}

RpcConnectionPool::~RpcConnectionPool() {
    stop();
}

void RpcConnectionPool::stop() {
    if (m_heartTimer) {
        m_heartTimer->cancel();
    }
    if (m_registry) {
        m_registry->close();
    }

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

    // 服务中心心跳定时器 30s
    m_heartTimer = acid::IOManager::GetThis()->addTimer(30'000, [this]{
        ACID_LOG_DEBUG(g_logger) << "heart beat";
        Protocol::ptr proto = Protocol::HeartBeat();
        Protocol::ptr response;

        {
            MutexType::Lock lock(m_sendMutex);
            m_registry->sendProtocol(proto);
            response = m_registry->recvProtocol();
        }

        if (!response) {
            ACID_LOG_WARN(g_logger) << "Registry close";
            //放弃服务中心
            m_heartTimer->cancel();
            m_heartTimer = nullptr;
        }
    }, true);

    return true;
}

std::vector<std::string> RpcConnectionPool::discover_impl(const std::string& name){
    std::vector<Result<std::string>> res;
    std::vector<std::string> rt;
    //Result<std::string> res;
    std::vector<Address::ptr> addrs;

    Protocol::ptr proto = Protocol::Create(Protocol::MsgType::RPC_SERVICE_DISCOVER, name);
    Protocol::ptr response;

    {
        MutexType::Lock lock(m_sendMutex);
        m_registry->sendProtocol(proto);
        response = m_registry->recvProtocol();
    }

    Serializer s(response->getContent());
    uint32_t cnt;
    s >> cnt;
    for (uint32_t i = 0; i < cnt; ++i) {
        Result<std::string> str;
        s >> str;
        res.push_back(str);
    }
    if (res.front().getCode() == RPC_NO_METHOD) {
        return std::vector<std::string>{};
    }

    for (size_t i = 0; i < res.size(); ++i) {
        rt.push_back(res[i].getVal());
    }
    return rt;
}

}