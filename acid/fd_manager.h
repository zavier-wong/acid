//
// Created by zavier on 2021/12/5.
//

#ifndef ACID_FD_MANAGER_H
#define ACID_FD_MANAGER_H

#include <memory>
#include "io_manager.h"
#include "mutex.h"
namespace acid{
class FdCtx : std::enable_shared_from_this<FdCtx>{
public:
    using ptr = std::shared_ptr<FdCtx>;
    FdCtx(int fd);
    ~FdCtx();
    bool init();
    bool isInit() const { return m_isInit;}
    bool isSocket() const { return m_isSocket;}
    bool isClose() const { return m_isClosed;}

    void setSysNonblock(bool f) { m_sysNonblock = f;}
    bool getSysNonblock() const { return m_sysNonblock;}

    void setUserNonblock(bool f) { m_userNonblock = f;}
    bool getUserNonblock() const { return m_userNonblock;}

    void setSendTimeout(uint64_t timeout) { m_sendTimeout = timeout;}
    uint64_t getSendTimeout() const { return m_sendTimeout;}

    void setRecvTimeout(uint64_t timeout) { m_sendTimeout = timeout;}
    uint64_t getRecvTimeout() const { return m_sendTimeout;}

    void setTimeout(int type, uint64_t timeout);
    uint64_t getTimeout(int type) const ;
private:
    bool m_isInit       : 1;
    bool m_isSocket     : 1;
    bool m_sysNonblock  : 1;
    bool m_userNonblock : 1;
    bool m_isClosed     : 1;
    int m_fd = 0;
    uint64_t m_sendTimeout;
    uint64_t m_recvTimeout;
    acid::IOManager* m_iomanager;
};

class FdManager{
public:
    using RWMutexType = acid::RWMutex;
    FdManager();
    FdCtx::ptr get(int fd, bool auto_create = false);
    void del(int fd);
private:
    RWMutexType m_mutex;
    std::vector<FdCtx::ptr> m_fds;
};

using FdMgr = Singleton<FdManager>;
}
#endif //ACID_FD_MANAGER_H
