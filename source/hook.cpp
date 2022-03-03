//
// Created by zavier on 2021/12/4.
//
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include "acid/config.h"
#include "acid/fiber.h"
#include "acid/fd_manager.h"
#include "acid/hook.h"
#include "acid/io_manager.h"
#include "acid/log.h"

static acid::Logger::ptr g_logger = ACID_LOG_NAME("system");

static acid::ConfigVar<int>::ptr g_tcp_connect_timeout =
        acid::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

namespace acid{
static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)

void hook_init(){
    static bool is_init = false;
    if (is_init){
        return;
    }
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    HOOK_FUN(XX)
#undef XX
}

static uint64_t s_connect_timout = -1;

struct _HookIniter{
    _HookIniter(){
        hook_init();
        s_connect_timout = g_tcp_connect_timeout->getValue();
        g_tcp_connect_timeout->addListener([](const int& old_val, const int& new_val){
            ACID_LOG_INFO(g_logger) << "tcp connect timeout changed from "
                                     << old_val << " to " << new_val;
            s_connect_timout = new_val;
        });
    }
};

static _HookIniter s_hook_initer;

bool is_enable_hook(){
    return t_hook_enable;
}

void set_hook_enable(bool flag){
    t_hook_enable = flag;
}

}

template<typename OriginFun, typename... Args>
ssize_t do_io(int fd, acid::IOManager::Event event, const char* func_name, OriginFun fun, Args&&... args){
    if(acid::t_hook_enable == false){
        return fun(fd, std::forward<Args>(args)...);
    }
    acid::FdCtx::ptr ctx = acid::FdMgr::GetInstance()->get(fd);
    if(!ctx){
        return fun(fd, std::forward<Args>(args)...);
    }
    if(ctx->isClose()) {
        errno = EBADF;
        return -1;
    }
    if(!ctx->isSocket() || ctx->getUserNonblock()) {
        return fun(fd, std::forward<Args>(args)...);
    }
    std::shared_ptr<int> timeCondition(new int{0});
    uint64_t timeout = 0;
    if(event == acid::IOManager::READ){
        timeout = ctx->getRecvTimeout();
    } else {
        timeout = ctx->getSendTimeout();
    }
retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while (n == -1 && errno == EINTR){
        n = fun(fd, std::forward<Args>(args)...);
    }
    if(n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)){
        acid::IOManager* ioManager = acid::IOManager::GetThis();
        ACID_ASSERT2(ioManager, "IOManager is not start");
        acid::Timer::ptr timer;
        std::weak_ptr<int> weakPtr(timeCondition);
        if( timeout != (uint64_t)-1 ){
            timer = ioManager->addConditionTimer(timeout,[weakPtr, ioManager, fd, event]{
                auto t = weakPtr.lock();
                if(!t || *t){
                    return ;
                }
                *t = ETIMEDOUT;
                ioManager->cancelEvent(fd,event);
            },weakPtr);
        }
        int rt = ioManager->addEvent(fd,event);
        if(!rt){
            ACID_LOG_ERROR(g_logger) << func_name << " addEvent(" << fd << ", " << event << ")";
            if(timer){
                timer->cancel();
            }
            return -1;
        }
        acid::Fiber::YieldToHold();
        if(timer){
            timer->cancel();
        }
        if(*timeCondition){
            errno = *timeCondition;
            return -1;
        }
        goto retry;
    }
    return n;
}

extern "C"{
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX);
#undef XX
unsigned int sleep(unsigned int seconds){
    if(acid::t_hook_enable == false){
        return sleep_f(seconds);
    }
    acid::Fiber::ptr fiber = acid::Fiber::GetThis();
    acid::IOManager* iom = acid::IOManager::GetThis();
    ACID_ASSERT2(iom, "IOManager is not start");
    iom->addTimer(seconds * 1000,[iom,fiber]() mutable {
        iom->submit(fiber, -1);
    });
    acid::Fiber::YieldToHold();
    return 0;
}
int usleep(useconds_t useconds){
    if(!acid::t_hook_enable){
        return sleep_f(useconds);
    }
    acid::Fiber::ptr fiber = acid::Fiber::GetThis();
    acid::IOManager* iom = acid::IOManager::GetThis();
    ACID_ASSERT2(iom, "IOManager is not start");
    iom->addTimer(useconds / 1000,[iom,fiber]() mutable {
        iom->submit(fiber, -1);
    });
    acid::Fiber::YieldToHold();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if(!acid::t_hook_enable) {
        return nanosleep_f(req, rem);
    }

    int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 /1000;
    acid::Fiber::ptr fiber = acid::Fiber::GetThis();
    acid::IOManager* iom = acid::IOManager::GetThis();
    ACID_ASSERT2(iom, "IOManager is not start");
    iom->addTimer(timeout_ms,[iom,fiber]() mutable {
        iom->submit(fiber, -1);
    });
    acid::Fiber::YieldToHold();
    return 0;
}

//socket
int socket(int domain, int type, int protocol){
    if(!acid::t_hook_enable){
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if(fd == -1){
        return fd;
    }
    acid::FdMgr::GetInstance()->get(fd, true);
    return fd;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
    return connect_with_timeout(sockfd, addr, addrlen, acid::s_connect_timout);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen){
    int fd = do_io(s, acid::IOManager::READ, "accept", accept_f, addr, addrlen);
    if(fd >= 0 && acid::t_hook_enable){
        acid::FdMgr::GetInstance()->get(fd, true);
    }
    return fd;
}

int close(int fd) {
    if(!acid::t_hook_enable){
        return close_f(fd);
    }
    acid::FdCtx::ptr fdCtx = acid::FdMgr::GetInstance()->get(fd);
    if(fdCtx){
        acid::IOManager* ioManager = acid::IOManager::GetThis();
        if(ioManager){
            ioManager->cancelAllEvent(fd);
        }
        acid::FdMgr::GetInstance()->del(fd);
    }
    return close_f(fd);
}

//read
ssize_t read(int fd, void *buf, size_t count) {
    return do_io(fd, acid::IOManager::READ, "read", read_f, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, acid::IOManager::READ, "readv", readv_f, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return do_io(sockfd, acid::IOManager::READ, "recv", recv_f, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    return do_io(sockfd, acid::IOManager::READ, "recvfrom", recvfrom_f, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, acid::IOManager::READ, "recvmsg", recvmsg_f, msg, flags);
}

//write
ssize_t write(int fd, const void *buf, size_t count) {
    return do_io(fd, acid::IOManager::WRITE, "write", write_f, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, acid::IOManager::WRITE, "writev", writev_f, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags) {
    return do_io(s, acid::IOManager::WRITE, "send", send_f, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    return do_io(s, acid::IOManager::WRITE, "send", sendto_f, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
    return do_io(s, acid::IOManager::WRITE, "sendmsg", sendmsg_f, msg,flags);
}

int fcntl(int fd, int cmd, ...){
    va_list va;
    va_start(va, cmd);
    switch(cmd) {
        case F_SETFL:
        {
            int arg = va_arg(va, int);
            va_end(va);
            acid::FdCtx::ptr ctx = acid::FdMgr::GetInstance()->get(fd);
            if(!ctx || ctx->isClose() || !ctx->isSocket()) {
                return fcntl_f(fd, cmd, arg);
            }
            ctx->setUserNonblock(arg & O_NONBLOCK);
            if(ctx->getSysNonblock()) {
                arg |= O_NONBLOCK;
            } else {
                arg &= ~O_NONBLOCK;
            }
            return fcntl_f(fd, cmd, arg);
        }
            break;
        case F_GETFL:
        {
            va_end(va);
            int arg = fcntl_f(fd, cmd);
            acid::FdCtx::ptr ctx = acid::FdMgr::GetInstance()->get(fd);
            if(!ctx || ctx->isClose() || !ctx->isSocket()) {
                return arg;
            }
            if(ctx->getUserNonblock()) {
                return arg | O_NONBLOCK;
            } else {
                return arg & ~O_NONBLOCK;
            }
        }
            break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        {
            int arg = va_arg(va, int);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        {
            va_end(va);
            return fcntl_f(fd, cmd);
        }
            break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
        {
            struct flock* arg = va_arg(va, struct flock*);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
        {
            struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
            break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
    }
}

int ioctl(int d, unsigned long int request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);

    if(request == FIONBIO) {
        bool user_nonblock = !!*(int*)arg;
        acid::FdCtx::ptr ctx = acid::FdMgr::GetInstance()->get(d);
        if(!ctx || ctx->isClose() || !ctx->isSocket()) {
            return ioctl_f(d, request, arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if(!acid::t_hook_enable) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if(level == SOL_SOCKET) {
        if(optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            acid::FdCtx::ptr ctx = acid::FdMgr::GetInstance()->get(sockfd);
            if(ctx) {
                const timeval* v = (const timeval*)optval;
                ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms){
    if(!acid::t_hook_enable) {
        return connect_f(fd, addr, addrlen);
    }
    acid::FdCtx::ptr ctx = acid::FdMgr::GetInstance()->get(fd);
    if(!ctx || ctx->isClose()) {
        errno = EBADF;
        return -1;
    }

    if(!ctx->isSocket()) {
        return connect_f(fd, addr, addrlen);
    }

    if(ctx->getUserNonblock()) {
        return connect_f(fd, addr, addrlen);
    }

    int n = connect_f(fd, addr, addrlen);
    if(n == 0) {
        return 0;
    } else if(n != -1 || errno != EINPROGRESS) {
        return n;
    }

    acid::IOManager* iom = acid::IOManager::GetThis();
    acid::Timer::ptr timer;
    std::shared_ptr<int> timeCondition(new int{0});
    std::weak_ptr<int> weakPtr(timeCondition);

    if(timeout_ms != (uint64_t)-1) {
        timer = iom->addConditionTimer(timeout_ms, [weakPtr, fd, iom]() {
            auto t = weakPtr.lock();
            if(!t || *t) {
                return;
            }
            *t = ETIMEDOUT;
            iom->cancelEvent(fd, acid::IOManager::WRITE);
        }, weakPtr);
    }

    int rt = iom->addEvent(fd, acid::IOManager::WRITE);
    if(rt) {
        acid::Fiber::YieldToHold();
        if(timer) {
            timer->cancel();
        }
        if(*timeCondition) {
            errno = *timeCondition;
            return -1;
        }
    } else {
        if(timer) {
            timer->cancel();
        }
        ACID_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
    }

    int error = 0;
    socklen_t len = sizeof(int);
    if(-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }
    if(!error) {
        return 0;
    } else {
        errno = error;
        return -1;
    }
}

}