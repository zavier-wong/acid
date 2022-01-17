//
// Created by zavier on 2021/12/14.
//

#ifndef ACID_TCP_SERVER_H
#define ACID_TCP_SERVER_H
#include <memory>
#include <functional>
#include "acid/io_manager.h"
#include "socket.h"
#include "acid/noncopyable.h"
namespace acid {
// enable_shared_from_this，本对象只能在堆上创建
class TcpServer : public std::enable_shared_from_this<TcpServer> , Noncopyable{
public:
    using ptr = std::shared_ptr<TcpServer>;
    TcpServer(IOManager* worker = IOManager::GetThis(), IOManager* accept_worker = IOManager::GetThis());
    virtual ~TcpServer();

    virtual bool bind(Address::ptr addr);
    virtual bool bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fail);

    virtual bool start();
    virtual void stop();

    uint64_t getRecvTimeout() const { return m_recvTimeout;}
    void setRecvTimeout(uint64_t timeout) { m_recvTimeout = timeout;}

    std::string getName() const { return m_name;}
    virtual void setName(const std::string& name) { m_name = name;}

    bool isStop() const { return m_isStop;}

protected:
    virtual void startAccept(Socket::ptr sock);
    virtual void handleClient(Socket::ptr client);
    /// 监听socket队列
    std::vector<Socket::ptr> m_listens;
    IOManager* m_worker;
    IOManager* m_acceptWorker;
    uint64_t m_recvTimeout;
    std::string m_name;
    bool m_isStop;
};

}
#endif //ACID_TCP_SERVER_H
