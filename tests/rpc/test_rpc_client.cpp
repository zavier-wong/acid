//
// Created by zavier on 2022/1/13.
//

#include <utility>

#include "acid/rpc/rpc_client.h"

using namespace acid;
using namespace rpc;

void test1() {
    Address::ptr address = Address::LookupAny("127.0.0.1:9000");
    RpcClient::ptr client(new RpcClient());
    if (!client->connect(address)) {
        SPDLOG_INFO(address->toString());
        return;
    }
    int n=0;
    while (n!=1000) {
        n++;
        client->callback("add", 0, n ,[](Result<int> res) {
            SPDLOG_INFO(res.toString());
        });
    }
    auto rt = client->call<int>("add", 0, n);
    SPDLOG_INFO(rt.toString());
    sleep(3);
    //client->close();
    client->setTimeout(1000);
    auto sl = client->call<void>("sleep");
    SPDLOG_INFO("sleep 2s {}", sl.toString());
    co_sched.Stop();
}

class ChannelListener : public PubsubListener {
public:
    ChannelListener(RpcClient::ptr client) : m_client(std::move(client)) {}
    void onSubscribe(const std::string& channel) override {
        SPDLOG_INFO("subscribe channel {}", channel);
    }
    void onUnsubscribe(const std::string& channel) override {
        SPDLOG_INFO("unsubscribe channel {}", channel);
    }
    void onMessage(const std::string& channel, const std::string& message) override {
        if (m_n++ > 5) {
            // 取消订阅
            m_client->unsubscribe(channel);
        }
        SPDLOG_INFO("channel {}, message {}", channel, message);
    }
private:
    int m_n = 0;
    RpcClient::ptr m_client;
};

void subscribe() {
    Address::ptr address = Address::LookupAny("127.0.0.1:9000");
    RpcClient::ptr client = std::make_shared<RpcClient>();

    if (!client->connect(address)) {
        SPDLOG_WARN(address->toString());
        return;
    }

    if (client->subscribe(std::make_shared<ChannelListener>(client), "number")) {
        SPDLOG_WARN("unsubscribe");
    } else {
        SPDLOG_WARN("connection error");
    }
}

class PatternListener : public PubsubListener {
public:
    PatternListener(RpcClient::ptr client) : m_client(std::move(client)) {}
    void onPatternSubscribe(const std::string& pattern) override {
        SPDLOG_INFO("subscribe pattern {}", pattern);
    }
    void onPatternUnsubscribe(const std::string& pattern) override {
        SPDLOG_INFO("unsubscribe pattern {}", pattern);
    }
    void onPatternMessage(const std::string& pattern, const std::string& channel, const std::string& message) override {
        if (m_n++ > 5) {
            // 取消订阅
            m_client->patternUnsubscribe(pattern);
        }
        SPDLOG_INFO("pattern {}, channel {}, message {}", pattern, channel, message);
    }
private:
    int m_n = 0;
    RpcClient::ptr m_client;
};

void pattern_subscribe() {
    Address::ptr address = Address::LookupAny("127.0.0.1:9000");
    RpcClient::ptr client = std::make_shared<RpcClient>();

    if (!client->connect(address)) {
        SPDLOG_WARN(address->toString());
        return;
    }

    // 模式订阅
    if (client->patternSubscribe(std::make_shared<PatternListener>(client), "number_*")) {
        SPDLOG_WARN("unsubscribe");
    } else {
        SPDLOG_WARN("connection error");
    }
}

void publish() {
    Address::ptr address = Address::LookupAny("127.0.0.1:9000");
    RpcClient::ptr client = std::make_shared<RpcClient>();

    if (!client->connect(address)) {
        SPDLOG_WARN(address->toString());
        return;
    }
    sleep(1);
    int n = 0;
    while (!client->isClose() && n < 10) {
        SPDLOG_INFO("publish {}", n);
        client->publish("number", std::to_string(n));
        client->publish("number_" + std::to_string(n), std::to_string(n));
        n++;
        sleep(3);
    }
    co_sched.Stop();
}

void test_pubsub() {
    go subscribe;
    go pattern_subscribe;
    go publish;
}

int main() {
    //go test1;
    go test_pubsub;
    co_sched.Start(0);
}
