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

enum Error {
    OK,
    NO_KEY,
    WRONG_LEADER,
    TIMEOUT,
};

inline std::string toString(Error err) {
    std::string str;
    switch (err) {
        case OK: str = "OK"; break;
        case NO_KEY: str = "No Key"; break;
        case WRONG_LEADER: str = "Wrong Leader"; break;
        case TIMEOUT: str = "Timeout"; break;
        default: str = "Unexpect Error";
    }
    return str;
}

inline Serializer& operator<<(Serializer& s, const Error& err) {
    int n = (int)err;
    s << n;
    return s;
}

inline Serializer& operator>>(Serializer& s, Error& err) {
    int n;
    s >> n;
    err = (Error)n;
    return s;
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

inline Serializer& operator<<(Serializer& s, const Operation& err) {
    int n = (int)err;
    s << n;
    return s;
}

inline Serializer& operator>>(Serializer& s, Operation& err) {
    int n;
    s >> n;
    err = (Operation)n;
    return s;
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
    friend Serializer& operator<<(Serializer& s, const CommandRequest& request) {
        s << request.operation << request.key << request.value << request.clientId << request.commandId;
        return s;
    }
    friend Serializer& operator>>(Serializer& s, CommandRequest& request) {
        s >> request.operation >> request.key >> request.value >> request.clientId >> request.commandId;
        return s;
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
    friend Serializer& operator<<(Serializer& s, const CommandResponse& response) {
        s << response.error << response.value << response.leaderId;
        return s;
    }
    friend Serializer& operator>>(Serializer& s, CommandResponse& response) {
        s >> response.error >> response.value >> response.leaderId;
        return s;
    }
};

}
#endif //ACID_COMMOM_H
