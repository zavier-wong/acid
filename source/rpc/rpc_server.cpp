//
// Created by zavier on 2022/1/13.
//

#include "acid/rpc/rpc_server.h"

namespace acid::rpc {
static Logger::ptr g_logger = ACID_LOG_NAME("system");
RpcServer::RpcServer(IOManager *worker, IOManager *accept_worker)
        : TcpServer(worker, accept_worker) {

}

void RpcServer::setName(const std::string &name) {
    TcpServer::setName(name);
}

bool RpcServer::bindRegistry(Address::ptr address) {
    m_registry = Socket::CreateTCP(address);
    if (!m_registry) {
        return false;
    }

    return m_registry->connect(address);
}

Protocol::ptr RpcServer::recvProtocol(Socket::ptr client) {
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

void RpcServer::sendProtocol(Socket::ptr client, Protocol::ptr p) {
    SocketStream::ptr stream = std::make_shared<SocketStream>(client, false);
    ByteArray::ptr byteArray = p->encode();
    stream->writeFixSize(byteArray, byteArray->getSize());
}

void RpcServer::handleClient(Socket::ptr client) {
    ACID_LOG_DEBUG(g_logger) << "handleClient: " << client->toString();

    while (true) {
        Protocol::ptr request = recvProtocol(client);
        if (!request) {
            break;
        }
        Protocol::ptr response = handleProtocol(request);
        if (!request) {
            break;
        }
        sendProtocol(client, response);
    }
}

Serializer::ptr RpcServer::call(const std::string &name, const std::string& arg) {
    Serializer::ptr serializer = std::make_shared<Serializer>();
    if (m_handlers.find(name) == m_handlers.end()) {
        (*serializer) << Result<>::code_type(RPC_NO_METHOD);
        (*serializer) << Result<>::msg_type("function not bind: " + name);
        return serializer;
    }
    auto fun = m_handlers[name];
    fun(serializer, arg);
    serializer->reset();
    return serializer;
}


Protocol::ptr RpcServer::handleProtocol(Protocol::ptr p) {
    Protocol::MsgType type = p->getMsgType();
    switch (type) {
        case Protocol::MsgType::RPC_METHOD_REQUEST:
            return handleMethodCall(p);
        case Protocol::MsgType::RPC_SERVICE_REGISTER:
            break;
        default:
            break;
    }

    return nullptr;
}

Protocol::ptr RpcServer::handleMethodCall(Protocol::ptr p) {
    Protocol::ptr response = std::make_shared<Protocol>();
    std::string func_name;
    Serializer request(p->getContent());
    request >> func_name;
    Serializer::ptr rt = call(func_name, request.toString());
    response->setMsgType(Protocol::MsgType::RPC_METHOD_RESPONSE);
    response->setContent(rt->toString());
    return response;
}

void RpcServer::registerService(const std::string &name) {
    Protocol::ptr proto = std::make_shared<Protocol>();
    proto->setMsgType(Protocol::MsgType::RPC_SERVICE_REGISTER);
    proto->setContent(name);

    sendProtocol(m_registry, proto);

    Protocol::ptr rp = recvProtocol(m_registry);
    if (!rp) {
        ACID_LOG_WARN(g_logger) << "registerService:" << name << " fail, registry socket:" << m_registry->toString();
        return;
    }
    Result<std::string> result;
    Serializer s(rp->getContent());
    s >> result;

    if (result.getCode() != RPC_SUCCESS) {
        ACID_LOG_WARN(g_logger) << result.toString();
    }
    ACID_LOG_DEBUG(g_logger) << result.toString();
}


}