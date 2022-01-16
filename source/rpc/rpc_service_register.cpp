//
// Created by zavier on 2022/1/16.
//

#include "acid/log.h"
#include "acid/rpc/rpc.h"
#include "acid/rpc/rpc_service_registry.h"

namespace acid::rpc {
static Logger::ptr g_logger = ACID_LOG_NAME("system");

RpcServiceRegistry::RpcServiceRegistry(IOManager *worker, IOManager *accept_worker)
        : TcpServer(worker, accept_worker) {
    setName("RpcServiceRegistry");
}

Protocol::ptr RpcServiceRegistry::recvRequest(Socket::ptr client) {
    SocketStream::ptr stream = std::make_shared<SocketStream>(client, false);
    Protocol::ptr proto = std::make_shared<Protocol>();
    ByteArray::ptr byteArray = std::make_shared<ByteArray>();
    if (stream->readFixSize(byteArray, proto->BASE_LENGTH) <= 0) {
        return nullptr;
    }
    byteArray->setPosition(0);
    proto->decodeMeta(byteArray);

    std::string buff;
    buff.resize(proto->getContentLength());

    if (stream->readFixSize(&buff[0], proto->getContentLength()) <= 0) {
        return nullptr;
    }
    proto->setContent(std::move(buff));
    return proto;
}

void RpcServiceRegistry::sendResponse(Socket::ptr client, Protocol::ptr p) {
    SocketStream::ptr stream = std::make_shared<SocketStream>(client, false);
    ByteArray::ptr byteArray = p->encode();
    stream->writeFixSize(byteArray, byteArray->getSize());
}

void RpcServiceRegistry::handleClient(Socket::ptr client) {
    ACID_LOG_DEBUG(g_logger) << "handleClient: " << client->toString();

    while (true) {
        Protocol::ptr request = recvRequest(client);
        if (!request) {
            handleUnregisterService(client);
            break;
        }
        Protocol::ptr response = handleRequest(request, client);
        if (!request) {
            break;
        }
        sendResponse(client, response);
    }
}

Protocol::ptr RpcServiceRegistry::handleRequest(Protocol::ptr p, Socket::ptr client) {
    Protocol::MsgType type = p->getMsgType();
    switch (type) {
        case Protocol::MsgType::RPC_SERVICE_REGISTER:
            return handleRegisterService(p, client);
        case Protocol::MsgType::RPC_SERVICE_DISCOVER:
            return handleDiscoverService(p);
        default:
            break;
    }

    return nullptr;
}
Protocol::ptr RpcServiceRegistry::handleRegisterService(Protocol::ptr p, Socket::ptr client) {
    std::string serviceAddress = client->getRemoteAddress()->toString();
    std::string serviceName = p->getContent();

    RWMutexType::WriteLock lock(m_mutex);
    auto it = m_services.emplace(serviceName, serviceAddress);
    m_iters[serviceAddress].push_back(it);
    lock.unlock();

    Result<std::string> res = Result<std::string>::Success();
    res.setVal(serviceName);

    Protocol::ptr proto = std::make_shared<Protocol>();
    proto->setMsgType(Protocol::MsgType::RPC_SERVICE_REGISTER_RESPONSE);
    Serializer s;
    s << res;
    s.reset();
    proto->setContent(s.toString());

    return proto;
}

void RpcServiceRegistry::handleUnregisterService(Socket::ptr sock) {
    RWMutexType::WriteLock lock(m_mutex);
    auto it = m_iters.find(sock->toString());
    if (it == m_iters.end()) {
        return;
    }
    auto its = it->second;
    for (auto& i: its) {
        m_services.erase(i);
    }

}

Protocol::ptr RpcServiceRegistry::handleDiscoverService(Protocol::ptr p) {
    std::string serviceName = p->getContent();
    Result<std::string> result;
    ByteArray byteArray;
    Protocol::ptr proto = std::make_shared<Protocol>();
    proto->setMsgType(Protocol::MsgType::RPC_SERVICE_DISCOVER_RESPONSE);

    RWMutexType::ReadLock lock(m_mutex);
    m_services.equal_range(serviceName);
    auto range = m_services.equal_range(serviceName);

    for (auto i = range.first; i != range.second; ++i) {
        byteArray.writeStringVint(i->second);
    }
    if (byteArray.getSize() == 0) {
        result = Result<std::string>::Fail();
    } else {
        byteArray.setPosition(0);
        result.setCode(RPC_SUCCESS);
        result.setMsg("discover service:" + serviceName);
        result.setVal(byteArray.toString());
    }
    Serializer s;
    s << result;
    s.reset();
    proto->setContent(s.toString());
    return proto;
}


}