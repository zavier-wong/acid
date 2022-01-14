//
// Created by zavier on 2021/12/5.
//

#include "acid/fd_manager.h"
#include "acid/hook.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace acid{
FdCtx::FdCtx(int fd)
    :m_isInit(false)
    ,m_isSocket(false)
    ,m_sysNonblock(false)
    ,m_userNonblock(false)
    ,m_isClosed(false)
    ,m_fd(fd)
    ,m_sendTimeout(-1)
    ,m_recvTimeout(-1){

    init();
}

FdCtx::~FdCtx() {

}

bool FdCtx::init() {
    if(m_isInit){
        return true;
    }
    m_sendTimeout = -1;
    m_recvTimeout = -1;
    struct stat fd_state;
    if(fstat(m_fd,&fd_state) == -1){
        m_isInit = false;
        m_isSocket = false;
    } else {
        m_isInit = true;
        m_isSocket = S_ISSOCK(fd_state.st_mode);
    }
    if(m_isSocket){
        int flags = fcntl_f(m_fd, F_GETFL, 0);
        if(!(flags & O_NONBLOCK)) {
            fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
        }
        m_sysNonblock = true;
    } else {
        m_sysNonblock = false;
    }
    m_userNonblock = false;
    m_isClosed = false;
    return m_isInit;
}

void FdCtx::setTimeout(int type, uint64_t timeout) {
    if(type == SO_RCVTIMEO) {
        m_recvTimeout = timeout;
    } else {
        m_sendTimeout = timeout;
    }
}

uint64_t FdCtx::getTimeout(int type) const {
    if(type == SO_RCVTIMEO) {
        return m_recvTimeout;
    } else {
        return m_sendTimeout;
    }
}

FdManager::FdManager() {
    m_fds.resize(64);
}

FdCtx::ptr FdManager::get(int fd, bool auto_create) {
    if(fd == -1) {
        return nullptr;
    }
    RWMutexType::ReadLock readLock(m_mutex);
    if((int)m_fds.size() <= fd){
        if(auto_create == false){
            return nullptr;
        }
    } else {
        if(m_fds[fd] || auto_create == false){
            return m_fds[fd];
        }
    }
    readLock.unlock();
    RWMutexType::WriteLock writeLock(m_mutex);
    if(fd >= (int)m_fds.size()) {
        m_fds.resize(fd * 1.5);
    }
    FdCtx::ptr fdCtx(new FdCtx(fd));
    m_fds[fd] = fdCtx;
    return fdCtx;
}

void FdManager::del(int fd) {
    RWMutexType::WriteLock writeLock(m_mutex);
    if((int)m_fds.size() <= fd) {
        return;
    }
    m_fds[fd].reset();
}

}