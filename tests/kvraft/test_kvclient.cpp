//
// Created by zavier on 2022/12/4.
//

#include "acid/kvraft/kvclient.h"

using namespace acid;
using namespace kvraft;

std::map<int64_t, std::string> servers = {
        {1, "localhost:7001"},
        {2, "localhost:7002"},
        {3, "localhost:7003"},
};

void Main() {
    KVClient client(servers);

    auto val = client.Get("name");
    SPDLOG_INFO("name: {}", val);

    client.Put("name", "zavier");
    val = client.Get("name");
    SPDLOG_INFO("name: {}", val);

    client.Append("name", " wong");
    val = client.Get("name");
    SPDLOG_INFO("name: {}", val);

    client.Put("name", "");
    val = client.Get("name");
    SPDLOG_INFO("name: {}", val);

    for (int i = 0; ;++i) {
        if (i % 10 == 0) {
            client.Put("save data", "");
        }
        client.Append("save data", std::to_string(i));
        val = client.Get("save data");
        SPDLOG_INFO("save data: {}", val);
        sleep(2);
    }
}

int main() {
    go Main;
    co_sched.Start();
}