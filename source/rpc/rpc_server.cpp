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

Serializer::ptr RpcServer::recvRequest(SocketStream::ptr client) {
    constexpr int LengthSize = 4;

    ByteArray::ptr byteArray = std::make_shared<ByteArray>();
    if (client->readFixSize(byteArray, LengthSize) <= 0) {
        return nullptr;
    }
    byteArray->setPosition(0);
    int length;
    length = byteArray->readFint32();
    byteArray->clear();

    if (client->readFixSize(byteArray, length) <= 0) {
        return nullptr;
    }

    byteArray->setPosition(0);
    return std::make_shared<Serializer>(byteArray);
}

void RpcServer::handleClient(Socket::ptr client) {
    ACID_LOG_DEBUG(g_logger) << "handleClient: " << client->toString();
    SocketStream::ptr stream = std::make_shared<SocketStream>(client);
    while (true) {
        Serializer::ptr request = recvRequest(stream);
        if (!request) {
            break;
        }
        ACID_LOG_DEBUG(g_logger) << request->getByteArray()->toHexString();
        std::string func_name;
        (*request) >> func_name;

        Serializer::ptr rt = call(func_name, request->toString());
        sendResponse(stream, rt);
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

void RpcServer::sendResponse(SocketStream::ptr client, Serializer::ptr s) {
    ByteArray::ptr byteArray = std::make_shared<ByteArray>();
    byteArray->writeFint32(s->size());
    byteArray->setPosition(0);

    if (client->writeFixSize(byteArray, byteArray->getSize()) < 0) {
        return;
    }

    client->writeFixSize(s->getByteArray(), s->size());
}


}