//
// Created by zavier on 2022/1/13.
//

#include "acid/rpc/rpc_server.h"

namespace acid::rpc {
static Logger::ptr g_logger = ACID_LOG_NAME("system");
RpcServer::RpcServer(IOManager *worker, IOManager *accept_worker)
        : TcpServer(worker, accept_worker) {

}
bool RpcServer::bind(Address::ptr address) {
    m_port = std::dynamic_pointer_cast<IPv4Address>(address)->getPort();
    return TcpServer::bind(address);
}

void RpcServer::setName(const std::string &name) {
    TcpServer::setName(name);
}

bool RpcServer::bindRegistry(Address::ptr address) {
    m_registry = Socket::CreateTCP(address);
    if (!m_registry) {
        return false;
    }
    Serializer s;
    s << m_port;
    s.reset();

    if (!m_registry->connect(address)) {
        ACID_LOG_WARN(g_logger) << "can't connect to registry";
        m_registry = nullptr;
        return false;
    }
    // 向服务中心声明为provider，注册服务端口
    Protocol::ptr proto = std::make_shared<Protocol>();
    proto->setMsgType(Protocol::MsgType::RPC_PROVIDER);
    proto->setContent(s.toString());
    sendProtocol(m_registry, proto);

    return true;
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

    if (!proto->getContentLength()) {
        return proto;
    }

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

bool RpcServer::start() {
    if (m_registry) {
        for(auto& item: m_handlers) {
            registerService(item.first);
        }
        auto server = std::dynamic_pointer_cast<RpcServer>(shared_from_this());
        // 服务中心心跳定时器 30s
        m_registry->setRecvTimeout(30'000);
        m_heartTimer = m_worker->addTimer(30'000, [server]{
            ACID_LOG_DEBUG(g_logger) << "heart beat";
            Protocol::ptr proto = std::make_shared<Protocol>();
            proto->setMsgType(Protocol::MsgType::HEARTBEAT_PACKET);
            server->sendProtocol(server->m_registry, proto);
            Protocol::ptr response = server->recvProtocol(server->m_registry);
            //ACID_LOG_DEBUG(g_logger) << response->toString();
            if (!response) {
                ACID_LOG_WARN(g_logger) << "Registry close";
                //server->stop();
                //放弃服务中心，独自提供服务
                server->m_heartTimer->cancel();
            }
        }, true);
    }
    return TcpServer::start();
}
void RpcServer::handleRegistry() {
    ACID_LOG_DEBUG(g_logger) << "handleRegistry: " << m_registry->toString();
    while (true) {
        Protocol::ptr request = recvProtocol(m_registry);
        if (!request) {
            break;
        }
        Protocol::ptr response;

        Protocol::MsgType type = request->getMsgType();
        switch (type) {
            case Protocol::MsgType::HEARTBEAT_PACKET:
                response = handleHeartbeatPacket(request);
                break;
            default:
                ACID_LOG_DEBUG(g_logger) << "protocol:" << request->toString();
                break;
        }

        if (!response) {
            break;
        }
        sendProtocol(m_registry, response);
    }
}
void RpcServer::handleClient(Socket::ptr client) {
    ACID_LOG_DEBUG(g_logger) << "handleClient: " << client->toString();

    while (true) {
        Protocol::ptr request = recvProtocol(client);
        if (!request) {
            break;
        }
        Protocol::ptr response;

        Protocol::MsgType type = request->getMsgType();
        switch (type) {
            case Protocol::MsgType::RPC_METHOD_REQUEST:
                response = handleMethodCall(request);
                break;
            default:
                ACID_LOG_DEBUG(g_logger) << "protocol:" << request->toString();
                break;
        }

        if (!response) {
            break;
        }
        sendProtocol(client, response);
    }
}

Serializer::ptr RpcServer::call(const std::string &name, const std::string& arg) {
    Serializer::ptr serializer = std::make_shared<Serializer>();
    if (m_handlers.find(name) == m_handlers.end()) {
        return serializer;
    }
    auto fun = m_handlers[name];
    fun(serializer, arg);
    serializer->reset();
    return serializer;
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
    ACID_LOG_INFO(g_logger) << result.toString();
}

Protocol::ptr RpcServer::handleHeartbeatPacket(Protocol::ptr p) {
    static Protocol::ptr Heartbeat = std::make_shared<Protocol>();
    Heartbeat->setMsgType(Protocol::MsgType::HEARTBEAT_PACKET);
    return Heartbeat;
}


}