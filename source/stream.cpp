//
// Created by zavier on 2021/12/15.
//

#include "acid/stream.h"

namespace acid {

ssize_t Stream::readFixSize(void *buffer, size_t length) {
    size_t offset = 0;
    size_t left = length;
    while (left > 0) {
        ssize_t n = read((char*)buffer + offset, left);
        if(n <= 0) {
            return n;
        }
        offset += n;
        left -= n;
    }
    return length;
}

ssize_t Stream::readFixSize(ByteArray::ptr buffer, size_t length) {
    size_t left = length;
    while (left > 0) {
        ssize_t n = read(buffer, left);
        if(n <= 0) {
            return n;
        }
        left -= n;
    }
    return length;
}

ssize_t Stream::writeFixSize(const void *buffer, size_t length) {
    size_t offset = 0;
    size_t left = length;
    while (left > 0) {
        ssize_t n = write((const char*)buffer + offset, left);
        if(n <= 0) {
            return n;
        }
        offset += n;
        left -= n;
    }
    return length;
}

ssize_t Stream::writeFixSize(ByteArray::ptr buffer, size_t length) {
    size_t offset = 0;
    size_t left = length;
    while (left > 0) {
        ssize_t n = write(buffer, left);
        if(n <= 0) {
            return n;
        }
        offset += n;
        left -= n;
    }
    return length;
}
}