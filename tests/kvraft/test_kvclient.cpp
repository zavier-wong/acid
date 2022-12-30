//
// Created by zavier on 2022/12/4.
//

#include "acid/kvraft/kvclient.h"

using namespace acid;
using namespace kvraft;

class MyListener : public PubsubListener {
public:
    void onSubscribe(const std::string& channel) override {
        SPDLOG_INFO("onSubscribe {}", channel);
    }
    void onMessage(const std::string& channel, const std::string& message) override {
        SPDLOG_INFO("Listener channel: [{}], message: [{}]", channel, message);
    }
    void onPatternMessage(const std::string& pattern, const std::string& channel, const std::string& message) override {
        SPDLOG_INFO("Listener pattern: [{}], channel: [{}], message: [{}]", pattern, channel, message);
    }
};

std::map<int64_t, std::string> servers = {
        {1, "localhost:7001"},
        {2, "localhost:7002"},
        {3, "localhost:7003"},
};

void Main() {
    sleep(2);
    KVClient::ptr client = std::make_shared<KVClient>(servers);
    client->setTimeout(-1);
    std::string val;

    client->Get("name", val);
    SPDLOG_INFO("name: {}", val);

    client->Put("name", "zavier");
    client->Get("name", val);
    SPDLOG_INFO("name: {}", val);

    client->Append("name", " wong");
    client->Get("name", val);
    SPDLOG_INFO("name: {}", val);

    client->Put("name", "");
    client->Get("name", val);
    SPDLOG_INFO("name: {}", val);

    for (int i = 0; ;++i) {
        if (i % 10 == 0) {
            client->Put("save data", "");
        }
        client->Append("save data", std::to_string(i));
        client->Get("save data", val);
        SPDLOG_INFO("save data: {}", val);
        sleep(2);
    }
}

void subscribe() {
    go [] {
        KVClient::ptr client = std::make_shared<KVClient>(servers);
        client->setTimeout(-1);
        std::string val;
        client->Get("name", val);
        if (!client->subscribe(std::make_shared<MyListener>(), TOPIC_KEYSPACE + "save data")) {
            SPDLOG_WARN("unsubscribe");
        }
    };

    go [] {
        KVClient::ptr client = std::make_shared<KVClient>(servers);
        client->setTimeout(-1);
        std::string val;
        client->Get("name", val);
        client->patternSubscribe(std::make_shared<MyListener>(), TOPIC_ALL_KEYEVENTS);
        SPDLOG_WARN("unsubscribe");
    };
};

int main() {
    go subscribe;
    go Main;
    co_sched.Start();
}