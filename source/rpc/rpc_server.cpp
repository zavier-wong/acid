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
    Socket::ptr sock = Socket::CreateTCP(address);

    if (!sock) {
        return false;
    }
    if (!sock->connect(address)) {
        ACID_LOG_WARN(g_logger) << "can't connect to registry";
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

bool RpcServer::start() {
    if (m_registry) {
        for(auto& item: m_handlers) {
            registerService(item.first);
        }
        auto server = std::dynamic_pointer_cast<RpcServer>(shared_from_this());
        // 服务中心心跳定时器 30s
        m_registry->getSocket()->setRecvTimeout(30'000);
        m_heartTimer = m_worker->addTimer(30'000, [server]{
            ACID_LOG_DEBUG(g_logger) << "heart beat";
            Protocol::ptr proto = Protocol::Create(Protocol::MsgType::HEARTBEAT_PACKET, "");
            server->m_registry->sendProtocol(proto);
            Protocol::ptr response = server->m_registry->recvProtocol();

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

void RpcServer::handleClient(Socket::ptr client) {
    ACID_LOG_DEBUG(g_logger) << "handleClient: " << client->toString();
    RpcSession::ptr session = std::make_shared<RpcSession>(client);
    Timer::ptr heartTimer;
    // 开启心跳定时器
    update(heartTimer, client);
    while (true) {
        Protocol::ptr request = session->recvProtocol();
        if (!request) {
            break;
        }
        // 更新定时器
        update(heartTimer, client);

        Protocol::ptr response;

        Protocol::MsgType type = request->getMsgType();
        switch (type) {
            case Protocol::MsgType::HEARTBEAT_PACKET:
                response = handleHeartbeatPacket(request);
                break;
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
        session->sendProtocol(response);
    }
}

void RpcServer::update(Timer::ptr& heartTimer, Socket::ptr client) {
    ACID_LOG_DEBUG(g_logger) << "update heart";
    if (!heartTimer) {
        heartTimer = m_worker->addTimer(m_AliveTime, [client]{
            ACID_LOG_DEBUG(g_logger) << "client:" << client->toString() << " closed";
            client->close();
        });
        return;
    }
    // 更新定时器
    heartTimer->reset(m_AliveTime, true);
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
    std::string func_name;
    Serializer request(p->getContent());
    request >> func_name;
    Serializer::ptr rt = call(func_name, request.toString());
    Protocol::ptr response = Protocol::Create(
            Protocol::MsgType::RPC_METHOD_RESPONSE, rt->toString(), p->getSequenceId());
    return response;
}

void RpcServer::registerService(const std::string &name) {
    Protocol::ptr proto = Protocol::Create(Protocol::MsgType::RPC_SERVICE_REGISTER, name);
    m_registry->sendProtocol(proto);

    Protocol::ptr rp = m_registry->recvProtocol();
    if (!rp) {
        ACID_LOG_WARN(g_logger) << "registerService:" << name << " fail, registry socket:" << m_registry->getSocket()->toString();
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
    return Protocol::HeartBeat();
}


}