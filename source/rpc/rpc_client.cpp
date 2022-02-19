//
// Created by zavier on 2022/2/16.
//
#include "acid/rpc/rpc_client.h"

static acid::Logger::ptr g_logger = ACID_LOG_NAME("system");

namespace acid::rpc {

RpcClient::RpcClient() { }

RpcClient::~RpcClient() {
    close();
}

void RpcClient::close() {
    ACID_LOG_DEBUG(g_logger) << "close";
    MutexType::Lock lock(m_mutex);

    if (m_isClose) {
        return;
    }

    m_isHearClose = true;
    m_isClose = true;
    m_queue.push(nullptr);

    for (auto i: m_responseHandle) {
        IOManager::GetThis()->submit(i.second.first);
    }

    if (m_heartTimer) {
        m_heartTimer->cancel();
        m_heartTimer = nullptr;
    }

    IOManager::GetThis()->delEvent(m_session->getSocket()->getSocket(), IOManager::READ);
    if (m_session->isConnected()) {
        m_session->close();
    }
}

bool RpcClient::connect(Address::ptr address){
    Socket::ptr sock = Socket::CreateTCP(address);

    if (!sock) {
        return false;
    }
    if (!sock->connect(address, m_timeout)) {
        m_session = nullptr;
        return false;
    }
    m_isHearClose = false;
    m_isClose = false;
    m_session = std::make_shared<RpcSession>(sock);
    IOManager::GetThis()->submit([this]{
        handleRecv();
    })->submit([this]{
        handleSend();
    });

    m_heartTimer = IOManager::GetThis()->addTimer(30'000, [this]{
        ACID_LOG_DEBUG(g_logger) << "heart beat";
        if (m_isHearClose || m_isClose) {
            ACID_LOG_DEBUG(g_logger) << "Server closed";
            close();
        }
        Protocol::ptr proto = Protocol::Create(Protocol::MsgType::HEARTBEAT_PACKET, "");
        Protocol::ptr response;
        {
            MutexType::Lock lock(m_sendMutex);
            m_session->sendProtocol(proto);
        }
        m_isHearClose = true;
    }, true);
    return true;
}

void RpcClient::setTimeout(uint64_t timeout_ms) {
    m_timeout = timeout_ms;
}

void RpcClient::handleSend() {
    while (true) {
        Protocol::ptr request;
        m_queue.waitAndPop(request);
        if (!request) {
            break;
        }
        {
            MutexType::Lock lock(m_sendMutex);
            m_session->sendProtocol(request);
        }
        if (m_isClose) {
            break;
        }
    }
}

void RpcClient::handleRecv() {
    if (!m_session->isConnected()) {
        return;
    }
    while (true) {
        Protocol::ptr response = m_session->recvProtocol();
        //ACID_LOG_WARN(g_logger) << "recv";
        if (!response) {
            //ACID_LOG_DEBUG(g_logger) << "close recv";
            if (m_isClose) {
                break;
            }

            break;
        }
        Protocol::MsgType type = response->getMsgType();
        switch (type) {
            case Protocol::MsgType::HEARTBEAT_PACKET:
                m_isHearClose = false;
                break;
            case Protocol::MsgType::RPC_METHOD_RESPONSE:
                handleMethodResponse(response);
                break;
            default:
                ACID_LOG_DEBUG(g_logger) << "protocol:" << response->toString();
                break;
        }
    }
    //ACID_LOG_DEBUG(g_logger) << "handle recv close";
}

void RpcClient::handleMethodResponse(Protocol::ptr response) {
    uint32_t id = response->getSequenceId();
    //ACID_LOG_DEBUG(g_logger) << "handle Method Response :" << id;
    std::map<uint32_t, std::pair<Fiber::ptr, Protocol::ptr>>::iterator it;
    {
        MutexType::Lock lock(m_mutex);
        it = m_responseHandle.find(id);
        if (it == m_responseHandle.end()) {
            return;
        }
    }

    Fiber::ptr fiber = it->second.first;
    it->second.second = response;
    IOManager::GetThis()->submit(fiber);

}

}