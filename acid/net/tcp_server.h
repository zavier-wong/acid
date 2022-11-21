//
// Created by zavier on 2021/12/14.
//

#ifndef ACID_TCP_SERVER_H
#define ACID_TCP_SERVER_H

#include <functional>
#include <libgo/libgo.h>
#include "socket.h"

namespace acid {

class TcpServer {
public:
    TcpServer(co::Scheduler* worker = &co_sched, co::Scheduler* accept_worker = &co_sched);
    virtual ~TcpServer();

    virtual bool bind(Address::ptr addr);
    virtual bool bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fail);

    // 阻塞线程
    virtual void start();
    virtual void stop();

    uint64_t getRecvTimeout() const { return m_recvTimeout;}
    void setRecvTimeout(uint64_t timeout) { m_recvTimeout = timeout;}

    std::string getName() const { return m_name;}
    virtual void setName(const std::string& name) { m_name = name;}

    bool isStop() const { return m_isStop;}

protected:
    virtual void startAccept(Socket::ptr sock);
    virtual void handleClient(Socket::ptr client);

protected:
    co::Scheduler* m_worker;
    co::Scheduler* m_acceptWorker;
    co_timer m_timer;
private:
    /// 监听socket队列
    std::vector<Socket::ptr> m_listens;
    uint64_t m_recvTimeout;
    std::string m_name;
    std::atomic_bool m_isStop;
};

}
#endif //ACID_TCP_SERVER_H
