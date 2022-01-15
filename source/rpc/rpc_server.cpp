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

Protocol::ptr RpcServer::recvRequest(SocketStream::ptr client) {
    Protocol::ptr proto = std::make_shared<Protocol>();
    ByteArray::ptr byteArray = std::make_shared<ByteArray>();
    if (client->readFixSize(byteArray, proto->BASE_LENGTH) <= 0) {
        return nullptr;
    }
    byteArray->setPosition(0);
    proto->decodeMeta(byteArray);

    std::string buff;
    buff.resize(proto->getContentLength());

    if (client->readFixSize(&buff[0], proto->getContentLength()) <= 0) {
        return nullptr;
    }
    proto->setContent(std::move(buff));
    return proto;
}

void RpcServer::handleClient(Socket::ptr client) {
    ACID_LOG_DEBUG(g_logger) << "handleClient: " << client->toString();
    SocketStream::ptr stream = std::make_shared<SocketStream>(client);
    while (true) {
        Protocol::ptr request = recvRequest(stream);
        if (!request) {
            break;
        }
        Protocol::ptr response = handleRequest(request);
        if (!request) {
            break;
        }
        sendResponse(stream, response);
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

void RpcServer::sendResponse(SocketStream::ptr client, Protocol::ptr p) {
    ByteArray::ptr byteArray = p->encode();
    client->writeFixSize(byteArray, byteArray->getSize());
}

Protocol::ptr RpcServer::handleRequest(Protocol::ptr p) {
    Protocol::MsgType type = p->getMsgType();
    switch (type) {
        case Protocol::MsgType::RPC_METHOD_REQUEST:
            return handleMethodCall(p);
        case Protocol::MsgType::RPC_METHOD_RESPONSE:
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

}