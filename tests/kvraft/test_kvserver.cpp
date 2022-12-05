//
// Created by zavier on 2022/12/4.
//
#include <string>
#include "acid/kvraft/kvserver.h"

using namespace acid;
using namespace rpc;
using namespace kvraft;

int64_t id;

std::map<int64_t, std::string> peers = {
        {1, "localhost:7001"},
        {2, "localhost:7002"},
        {3, "localhost:7003"},
};

void Main() {
    // raft状态和快照存放的地方
    Persister::ptr persister = std::make_shared<Persister>(fmt::format("kvserver-{}", id));
    KVServer server(peers, id, persister);
    server.start();
}

// 启动方法
// ./test_kvserver 1
// ./test_kvserver 2
// ./test_kvserver 3
// 只要启动任意两个节点就可以运行分布式KV存储

int main(int argc, char** argv) {
    if (argc <= 1) {
        SPDLOG_ERROR("please input kvserver id");
        return 0;
    } else {
        SPDLOG_INFO("argv[1] = {}", argv[1]);
        id = std::stoll(argv[1]);
    }
    go Main;
    co_sched.Start();
}