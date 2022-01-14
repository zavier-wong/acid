//
// Created by zavier on 2021/12/15.
//

#include "acid/net/socket_stream.h"

namespace acid {

SocketStream::SocketStream(Socket::ptr socket, bool owner)
    : m_socket(socket)
    , m_isOwner(owner){

}

SocketStream::~SocketStream() {
    if(m_socket && m_isOwner) {
        m_socket->close();
    }
}

ssize_t SocketStream::read(void *buffer, size_t length) {
    if (!isConnected()) {
        return -1;
    }
    return m_socket->recv(buffer, length);
}

ssize_t SocketStream::read(ByteArray::ptr buffer, size_t length) {
    if (!isConnected()) {
        return -1;
    }
    std::vector<iovec> iovs;
    buffer->getWriteBuffers(iovs, length);
    ssize_t n =m_socket->recv(&iovs[0], iovs.size());
    if(n > 0) {
        buffer->setPosition(buffer->getPosition() + n);
    }
    return n;
}

ssize_t SocketStream::write(const void *buffer, size_t length) {
    if (!isConnected()) {
        return -1;
    }
    return m_socket->send(buffer, length);
}

ssize_t SocketStream::write(ByteArray::ptr buffer, size_t length) {
    if (!isConnected()) {
        return -1;
    }
    std::vector<iovec> iovs;
    buffer->getReadBuffers(iovs, length);
    ssize_t n = m_socket->send(&iovs[0], iovs.size());
    if(n > 0) {
        buffer->setPosition(buffer->getPosition() + n);
    }
    return n;
}

void SocketStream::close() {
    if (m_socket) {
        m_socket->close();
    }
}


}