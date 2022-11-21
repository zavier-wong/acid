//
// Created by zavier on 2021/12/15.
//

#ifndef ACID_STREAM_H
#define ACID_STREAM_H
#include <memory>
#include "acid/common/byte_array.h"
namespace acid {
class Stream {
public:
    using ptr = std::shared_ptr<Stream>;
    virtual ~Stream() {};
    virtual ssize_t read(void* buffer, size_t length) = 0;
    virtual ssize_t read(ByteArray::ptr buffer, size_t length) = 0;

    virtual ssize_t write(const void* buffer, size_t length) = 0;
    virtual ssize_t write(ByteArray::ptr buffer, size_t length) = 0;

    virtual void close() = 0;

    ssize_t readFixSize(void* buffer, size_t length);
    ssize_t readFixSize(ByteArray::ptr buffer, size_t length);

    ssize_t writeFixSize(const void* buffer, size_t length);
    ssize_t writeFixSize(ByteArray::ptr buffer, size_t length);
private:

};
}
#endif //ACID_STREAM_H
