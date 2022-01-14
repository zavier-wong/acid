//
// Created by zavier on 2021/12/14.
//

#include "acid/config.h"
#include "acid/log.h"
#include "acid/net/tcp_server.h"

namespace acid {
static Logger::ptr g_logger = ACID_LOG_NAME("system");

static ConfigVar<uint64_t>::ptr g_tcp_server_recv_timeout =
        Config::Lookup<uint64_t>("tcp_server.recv_timeout",
                                 (uint64_t)(60 * 1000 * 2), "tcp server recv timeout");

TcpServer::TcpServer(IOManager *worker, IOManager *accept_worker)
    : m_worker(worker)
    , m_acceptWorker(accept_worker)
    , m_recvTimeout(g_tcp_server_recv_timeout->getValue())
    , m_name("acid/1.0.0")
    , m_isStop(true){

}

TcpServer::~TcpServer() {
    //stop();
}

bool TcpServer::bind(Address::ptr addr) {
    std::vector<Address::ptr> addrs, fail;
    addrs.push_back(addr);
    return bind(addrs, fail);
}

bool TcpServer::bind(const std::vector<Address::ptr> &addrs, std::vector<Address::ptr> &fail) {
    for(Address::ptr addr : addrs){
        Socket::ptr sock = Socket::CreateTCP(addr);
        if(!sock->bind(addr)) {
            ACID_LOG_ERROR(g_logger)  << "bind fail errno="
                                      << errno << " errstr=" << strerror(errno)
                                      << " addr=[" << addr->toString() << "]";
            fail.push_back(addr);
            continue;
        }
        if(!sock->listen()) {
            ACID_LOG_ERROR(g_logger)  << "listen fail errno="
                                      << errno << " errstr=" << strerror(errno)
                                      << " addr=[" << addr->toString() << "]";
            fail.push_back(addr);
            continue;
        }
        m_listens.push_back(sock);
    }
    if(!fail.empty()) {
        m_listens.clear();
        return false;
    }
    for(auto& sock : m_listens) {
        ACID_LOG_INFO(g_logger) << "Server name=" << m_name << " bind:" << sock->toString() << " success";
    }

    return true;
}

bool TcpServer::start() {
    //m_worker->addTimer(-1, []{}, true);
    if(!isStop()) {
        return false;
    }
    m_isStop = false;
    TcpServer::ptr self = shared_from_this();
    for(auto& sock : m_listens) {
        m_acceptWorker->submit([self, sock] {
            //ACID_LOG_DEBUG(g_logger) << "acceptWorker->submit";
            self->startAccept(sock);
        });
    }
    ACID_LOG_DEBUG(g_logger) << "TcpServer::start()";
    //m_acceptWorker->addTimer(-1, []{}, true);
    //m_worker->addTimer(-1, []{}, true);
    return true;
}

void TcpServer::stop() {
    if(isStop()){
        return;
    }
    m_isStop = true;

    TcpServer::ptr self = shared_from_this();

    m_acceptWorker->submit([self, this]{
        for(auto& sock : m_listens) {
            sock->cancelAll();
            sock->close();
        }
    });
}

void TcpServer::startAccept(Socket::ptr sock) {
    TcpServer::ptr self = shared_from_this();
    while(!isStop()) {
        Socket::ptr client = sock->accept();
        if(client) {
            client->setRecvTimeout(m_recvTimeout);
            m_worker->submit([self, client]{
                self->handleClient(client);
            });
        } else {
            ACID_LOG_ERROR(g_logger) << "accept fail, errno=" << errno << " errstr=" << strerror(errno);
        }
    }
}

void TcpServer::handleClient(Socket::ptr client) {
    ACID_LOG_INFO(g_logger) << "handleClient: " << client->toString();
}

}