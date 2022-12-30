//
// Created by zavier on 2022/12/21.
//

#ifndef ACID_PUBSUB_H
#define ACID_PUBSUB_H

#include <functional>
#include <memory>
#include <string>

namespace acid::rpc {
enum class PubsubMsgType {
    Publish,
    Message,
    Subscribe,
    Unsubscribe,
    PatternMessage,
    PatternSubscribe,
    PatternUnsubscribe,
};

struct PubsubRequest {
    PubsubMsgType type;
    std::string channel;
    std::string message;
    std::string pattern;
};

struct PubsubResponse {
    PubsubMsgType type;
    std::string channel;
    std::string pattern;
};

// 发布订阅监听器
class PubsubListener {
public:
    using ptr = std::shared_ptr<PubsubListener>;

    virtual ~PubsubListener() {}
    /**
     * @param channel 频道
     * @param message 消息
     */
    virtual void onMessage(const std::string& channel, const std::string& message) {}
    /**
     * @param channel 频道
     */
    virtual void onSubscribe(const std::string& channel) {}
    /**
     * @param channel 频道
     */
    virtual void onUnsubscribe(const std::string& channel) {}

    /**
     * @param pattern 模式
     * @param channel 频道
     * @param message 消息
     */
    virtual void onPatternMessage(const std::string& pattern, const std::string& channel, const std::string& message) {}
    /**
     * @param pattern 模式
     */
    virtual void onPatternSubscribe(const std::string& pattern) {}
    /**
     * @param pattern 模式
     */
    virtual void onPatternUnsubscribe(const std::string& pattern) {}
};

}
#endif //ACID_PUBSUB_H