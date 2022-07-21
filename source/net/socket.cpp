//
// Created by zavier on 2021/12/6.
//
#include <netinet/tcp.h>
#include "acid/fd_manager.h"
#include "acid/hook.h"
#include "acid/log.h"
#include "acid/macro.h"
#include "acid/net/socket.h"

namespace acid{
static Logger::ptr g_logger = ACID_LOG_NAME("system");

Socket::Socket(int family, int type, int protocol)
        :m_sock(-1)
        ,m_family(family)
        ,m_type(type)
        ,m_protocol(protocol){

}

Socket::~Socket() {
    close();
}
void Socket::setSendTimeout(uint64_t t) {
    struct timeval tv = {int(t/1000), int(t % 1000 *1000)};
    setOption(SOL_SOCKET, SO_SNDTIMEO, &tv);
}
uint64_t Socket::getSendTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
    if(ctx){
        return ctx->getSendTimeout();
    }
    return -1;
}

void Socket::setRecvTimeout(uint64_t t) {
    struct timeval tv = {int(t/1000), int(t % 1000 *1000)};
    setOption(SOL_SOCKET, SO_RCVTIMEO, &tv);
}

uint64_t Socket::getRecvTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(m_sock);
    if(ctx){
        return ctx->getRecvTimeout();
    }
    return -1;
}

bool Socket::getOption(int level, int option, void* result, size_t* len) {
    int rt = getsockopt(m_sock, level, option, result, (socklen_t*)len);
    if(rt){
        ACID_LOG_DEBUG(g_logger) << "getOption sock=" << m_sock
                                 << " level=" << level << " option=" << option
                                 << " errno=" << errno << " errorstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::setOption(int level, int option, void* result, size_t len){
    int rt = setsockopt(m_sock, level, option, result, (socklen_t)len);
    if(rt){
        ACID_LOG_ERROR(g_logger) << "setOption sock=" << m_sock
                << " level=" << level << " option=" << option
                << " errno=" << errno << " errorstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::init(int sock) {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock);
    if(ctx && ctx->isSocket() && !ctx->isClose()){
        m_sock = sock;
        m_isConnected = true;
        initSocket();
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false;
}

void Socket::initSocket() {
    int val = 1;
    setOption(SOL_SOCKET, SO_REUSEADDR, &val);
    if(m_type == SOCK_STREAM){
        setOption(IPPROTO_TCP, TCP_NODELAY, &val);
    }
}

void Socket::newSocket() {
    m_sock = socket(m_family, m_type, m_protocol);
    if(ACID_LIKELY(m_sock != -1)){
        initSocket();
    } else {
        ACID_LOG_ERROR(g_logger) << "socket(" << m_family << ", " << m_type << ", " << m_protocol << ") "
                            << "errno=" << errno << " errstr=" << strerror(errno);
    }
}
Socket::ptr Socket::accept() {
    Socket::ptr sock(std::make_shared<Socket>(m_family, m_type, m_protocol));
    int newsock = ::accept(m_sock, nullptr, nullptr);
    if(newsock < 0){
        ACID_LOG_DEBUG(g_logger) << "accept(" << m_sock << ") "
                << "errno=" << errno << "errstr=" << strerror(errno);
        return nullptr;
    }
    if(sock->init(newsock)){
        return sock;
    }
    return nullptr;
}

bool Socket::bind(const Address::ptr address) {
    if(ACID_UNLIKELY(!isValid())){
        newSocket();
        if(ACID_UNLIKELY(!isValid())){
            return false;
        }
    }

    if(ACID_UNLIKELY(m_family != address->getFamily())){
        ACID_LOG_ERROR(g_logger) << "bind sock.family(" << m_family << ") address.family(" << address->getFamily() << ")"
            << " not equal, address=" << address->toString();
        return false;
    }

    if(::bind(m_sock, address->getAddr(), address->getAddrLen())){
        ACID_LOG_ERROR(g_logger) << "bind address=" << address->toString() << " error, errno="
                << errno << " errstr=" << strerror(errno);
        return false;
    }
    getLocalAddress();
    return true;
}

bool Socket::connect(const Address::ptr address, uint64_t timeout_ms) {
    if(ACID_UNLIKELY(!isValid())){
        newSocket();
        if(ACID_UNLIKELY(!isValid())){
            return false;
        }
    }

    if(ACID_UNLIKELY(m_family != address->getFamily())){
        ACID_LOG_WARN(g_logger) << "connect sock.family(" << m_family << ") address.family(" << address->getFamily() << ")"
                                 << " not equal, address=" << address->toString();
        return false;
    }

    if(timeout_ms == (uint64_t)-1){
        if(::connect(m_sock, address->getAddr(), address->getAddrLen())){
            ACID_LOG_WARN(g_logger) << "sock=" << m_sock << " connect(" << address->toString() << ") error, errno="
                                     << errno << " errstr=" << strerror(errno);
            close();
            return false;
        }
    } else {
        if(::connect_with_timeout(m_sock, address->getAddr(), address->getAddrLen(), timeout_ms)){
            ACID_LOG_WARN(g_logger) << "sock=" << m_sock << " connect(" << address->toString() << ") error, timeout="
                                    << timeout_ms << " errno=" << errno << " errstr=" << strerror(errno);
            close();
            return false;
        }
    }
    m_isConnected = true;
    getLocalAddress();
    getRemoteAddress();
    return true;
}

bool Socket::listen(int backlog){
    if(ACID_UNLIKELY(!isValid())){
        ACID_LOG_ERROR(g_logger) << "listen error, sock=-1";
        return false;
    }
    if(::listen(m_sock, backlog)){
        ACID_LOG_ERROR(g_logger) << "listen error, errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::close(){
    if(m_isConnected == false && m_sock == -1){
        return true;
    }
    m_isConnected = false;
    if(m_sock != -1){
        ::close(m_sock);
        m_sock = -1;
    }
    return true;
}

ssize_t Socket::send(const void* buffer, size_t length, int flags) {
    if(isConnected()) {
        return ::send(m_sock, buffer, length, flags);
    }
    return -1;
}

ssize_t Socket::send(const iovec* buffers, size_t length, int flags) {
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        return ::sendmsg(m_sock, &msg, flags);
    }
    return -1;
}

ssize_t Socket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags) {
    if(isConnected()) {
        return ::sendto(m_sock, buffer, length, flags, to->getAddr(), to->getAddrLen());
    }
    return -1;
}

ssize_t Socket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags) {
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        msg.msg_name = to->getAddr();
        msg.msg_namelen = to->getAddrLen();
        return ::sendmsg(m_sock, &msg, flags);
    }
    return -1;
}

ssize_t Socket::recv(void* buffer, size_t length, int flags) {
    if(isConnected()) {
        return ::recv(m_sock, buffer, length, flags);
    }
    return -1;
}

ssize_t Socket::recv(iovec* buffers, size_t length, int flags) {
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

ssize_t Socket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags) {
    if(isConnected()) {
        socklen_t len = from->getAddrLen();
        return ::recvfrom(m_sock, buffer, length, flags, from->getAddr(), &len);
    }
    return -1;
}

ssize_t Socket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags) {
    if(isConnected()) {
        msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = (iovec*)buffers;
        msg.msg_iovlen = length;
        msg.msg_name = from->getAddr();
        msg.msg_namelen = from->getAddrLen();
        return ::recvmsg(m_sock, &msg, flags);
    }
    return -1;
}

Address::ptr Socket::getRemoteAddress() {
    if(m_remoteAddress){
        return m_remoteAddress;
    }
    Address::ptr result;
    switch (m_family) {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;;
        default:
            result.reset(new UnknownAddress(m_family));
    }
    socklen_t addrlen = result->getAddrLen();
    if(getpeername(m_sock, result->getAddr(), &addrlen)){
        return Address::ptr (new UnknownAddress(m_family));
    }
    if(m_family == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }

    m_remoteAddress = result;
    return m_remoteAddress;
}

Address::ptr Socket::getLocalAddress() {
    if(m_localAddress){
        return m_localAddress;
    }
    Address::ptr result;
    switch (m_family) {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;;
        default:
            result.reset(new UnknownAddress(m_family));
    }
    socklen_t addrlen = result->getAddrLen();
    if(getsockname(m_sock, result->getAddr(), &addrlen)){
        return Address::ptr (new UnknownAddress(m_family));
    }
    if(m_family == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(addrlen);
    }

    m_localAddress = result;
    return m_localAddress;
}

int Socket::getError() {
    int error;
    size_t errlen = sizeof(int);
    if(getOption(SOL_SOCKET, SO_ERROR, &error, &errlen)){
        return -1;
    }
    return error;
}

std::ostream &Socket::dump(std::ostream &os) const {
    os << "[socket sock=" << m_sock
        << " isConnected=" << m_isConnected
        << " family=" << m_family
        << " type=" << m_type
        << " protocol=" << m_protocol;
    if(m_localAddress){
        os << " localAddress=" << m_localAddress->toString();
    }
    if(m_remoteAddress){
        os << " remoteAddress=" << m_remoteAddress->toString();
    }
    os << "]";
    return os;
}

std::string Socket::toString() const{
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

bool Socket::cancelRead() {
    return IOManager::GetThis()->cancelEvent(m_sock, IOManager::READ);
}

bool Socket::cancelWrite() {
    return IOManager::GetThis()->cancelEvent(m_sock, IOManager::WRITE);
}

bool Socket::cancelAccept() {
    return IOManager::GetThis()->cancelEvent(m_sock, IOManager::READ);
}

bool Socket::cancelAll() {
    return IOManager::GetThis()->cancelAllEvent(m_sock);
}

Socket::ptr Socket::CreateTCPSocket() {
    Socket::ptr res(new Socket(IPv4, TCP, 0));
    return res;
}

Socket::ptr Socket::CreateUDPSocket() {
    Socket::ptr res(new Socket(IPv4, UDP, 0));
    return res;
}

Socket::ptr Socket::CreateTCPSocket6() {
    Socket::ptr res(new Socket(IPv6, TCP, 0));
    return res;
}

Socket::ptr Socket::CreateUDPSocket6() {
    Socket::ptr res(new Socket(IPv6, UDP, 0));
    return res;
}

Socket::ptr Socket::CreateUnixTCPSocket() {
    Socket::ptr res(new Socket(UNIX, TCP, 0));
    return res;
}

Socket::ptr Socket::CreateUnixUDPSocket() {
    Socket::ptr res(new Socket(UNIX, UDP, 0));
    return res;
}

Socket::ptr Socket::CreateUDP(Address::ptr address) {
    Socket::ptr res(new Socket(address->getFamily(), UDP, 0));
    return res;
}

Socket::ptr Socket::CreateTCP(Address::ptr address) {
    Socket::ptr res(new Socket(address->getFamily(), TCP, 0));
    return res;
}


}