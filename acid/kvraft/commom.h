//
// Created by zavier on 2022/12/3.
//

#ifndef ACID_COMMOM_H
#define ACID_COMMOM_H

#include <string>
#include <inttypes.h>
#include <fmt/format.h>
#include "../rpc/serializer.h"

namespace acid::kvraft {
using namespace acid::rpc;

inline const std::string COMMAND = "KVServer::handleCommand";

inline const std::string KEYEVENTS_PUT = "put";
inline const std::string KEYEVENTS_DEL = "del";
inline const std::string KEYEVENTS_APPEND = "append";
inline const std::string TOPIC_KEYEVENT = "__keyevent__:";
inline const std::string TOPIC_KEYSPACE = "__keyspace__:";
// 所有 key 的事件
inline const std::string TOPIC_ALL_KEYEVENTS = "__keyevent__:*";
// 所有 key 的 put 事件
inline const std::string TOPIC_KEYEVENTS_PUT = TOPIC_KEYEVENT + KEYEVENTS_PUT;
// 所有 key 的 del 事件
inline const std::string TOPIC_KEYEVENTS_DEL = TOPIC_KEYEVENT + KEYEVENTS_DEL;
// 所有 key 的 append 事件
inline const std::string TOPIC_KEYEVENTS_APPEND = TOPIC_KEYEVENT + KEYEVENTS_APPEND;

enum Error {
    OK,
    NO_KEY,
    WRONG_LEADER,
    TIMEOUT,
    CLOSED,
};

inline std::string toString(Error err) {
    std::string str;
    switch (err) {
        case OK: str = "OK"; break;
        case NO_KEY: str = "No Key"; break;
        case WRONG_LEADER: str = "Wrong Leader"; break;
        case TIMEOUT: str = "Timeout"; break;
        case CLOSED: str = "Closed"; break;
        default: str = "Unexpect Error";
    }
    return str;
}

enum Operation {
    GET,
    PUT,
    APPEND,
    DELETE,
    CLEAR,
};

inline std::string toString(Operation op) {
    std::string str;
    switch (op) {
        case GET: str = "GET"; break;
        case PUT: str = "PUT"; break;
        case APPEND: str = "APPEND"; break;
        case DELETE: str = "DELETE"; break;
        case CLEAR: str = "CLEAR"; break;
        default: str = "Unexpect Operation";
    }
    return str;
}

struct CommandRequest {
    Operation operation;
    std::string key;
    std::string value;
    int64_t clientId;
    int64_t commandId;
    std::string toString() const {
        std::string str = fmt::format("operation: {}, key: {}, value: {}, clientId: {}, commandId: {}",
                                      kvraft::toString(operation), key, value, clientId, commandId);
        return "{" + str + "}";
    }
};

struct CommandResponse {
    Error error = OK;
    std::string value;
    int64_t leaderId = -1;
    std::string toString() const {
        std::string str = fmt::format("error: {}, value: {}, leaderId: {}", kvraft::toString(error), value, leaderId);
        return "{" + str + "}";
    }
};

}
#endif //ACID_COMMOM_H
