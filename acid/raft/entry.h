//
// Created by zavier on 2022/7/8.
//

#ifndef ACID_ENTRY_H
#define ACID_ENTRY_H

#include "../rpc/serializer.h"

namespace acid::raft {

/**
 * @brief 日志条目，一条日志
 */
struct Entry {
    enum EntryType {
        Normal = 0,     // 常规日志
        Dummy,          // 日志索引从1开始，所以给第0条日志用虚拟entry填充
    };
    // 日志索引
    int64_t index = 0;
    // 日志任期
    int64_t term = 0;
    // 日志类型
    EntryType type = Normal;
    // 日志内容data是一个二进制类型，
    // 使用者负责把业务序列化成二进制数，
    // 在apply日志的时候再反序列化执行相应业务操作
    std::string data;
    friend rpc::Serializer &operator<<(rpc::Serializer &s, const Entry &entry) {
        s << entry.index << entry.term << entry.type << entry.data;
        return s;
    }
    friend rpc::Serializer &operator>>(rpc::Serializer &s, Entry &entry) {
        s >> entry.index >> entry.term >> entry.type >> entry.data;
        return s;
    }
};
}
#endif //ACID_ENTRY_H
