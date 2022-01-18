//
// Created by zavier on 2022/1/16.
//

#include "acid/log.h"
#include "acid/rpc/rpc_connection_poll.h"

namespace acid::rpc {
static Logger::ptr g_logger = ACID_LOG_NAME("system");

RpcConnectionPool::RpcConnectionPool(size_t capacity,uint64_t timeout_ms)
        : m_timeout(timeout_ms)
        , m_conns(capacity){
    m_registry = Socket::CreateTCPSocket();
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
    if (!m_registry->connect(address, m_timeout)) {
        ACID_LOG_ERROR(g_logger) << "connect to register fail";
        return false;
    }
    ACID_LOG_DEBUG(g_logger) << "connect to registry: " << m_registry->toString();
    // auto self = shared_from_this();
    // 服务中心心跳定时器 30s
    m_heartTimer = acid::IOManager::GetThis()->addTimer(30'000, [this]{
        ACID_LOG_DEBUG(g_logger) << "heart beat";
        Protocol::ptr proto = std::make_shared<Protocol>();
        proto->setMsgType(Protocol::MsgType::HEARTBEAT_PACKET);
        Protocol::ptr response;

        {
            MutexType::Lock lock(m_mutex);
            sendRequest(proto);
            response = recvResponse();
        }
        //ACID_LOG_DEBUG(g_logger) << response->toString();
        if (!response) {
            ACID_LOG_WARN(g_logger) << "Registry close";
            //放弃服务中心
            m_heartTimer->cancel();
            m_heartTimer = nullptr;
        }
    }, true);

    return true;
}

Protocol::ptr RpcConnectionPool::recvResponse() {
    SocketStream::ptr stream = std::make_shared<SocketStream>(m_registry, false);
    Protocol::ptr response = std::make_shared<Protocol>();

    ByteArray::ptr byteArray = std::make_shared<ByteArray>();

    if (stream->readFixSize(byteArray, response->BASE_LENGTH) <= 0) {
        return nullptr;
    }

    byteArray->setPosition(0);
    response->decodeMeta(byteArray);

    if (response->getContentLength() == 0) {
        response->setContent("");
        return response;
    }

    std::string buff;
    buff.resize(response->getContentLength());

    if (stream->readFixSize(&buff[0], buff.size()) <= 0) {
        return nullptr;
    }
    response->setContent(std::move(buff));
    return response;
}

bool RpcConnectionPool::sendRequest(Protocol::ptr p) {
    SocketStream::ptr stream = std::make_shared<SocketStream>(m_registry, false);
    ByteArray::ptr bt = p->encode();
    if (stream->writeFixSize(bt, bt->getSize()) < 0) {
        return false;
    }
    return true;
}
std::vector<std::string> RpcConnectionPool::discover(const std::string& name){
    std::vector<Result<std::string>> res;
    std::vector<std::string> rt;
    //Result<std::string> res;
    std::vector<Address::ptr> addrs;
    Protocol::ptr proto = std::make_shared<Protocol>();
    proto->setMsgType(Protocol::MsgType::RPC_SERVICE_DISCOVER);
    proto->setContent(name);
    Protocol::ptr response;

    {
        MutexType::Lock lock(m_mutex);
        sendRequest(proto);
        response = recvResponse();
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