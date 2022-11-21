//
// Created by zavier on 2022/1/16.
//

#include "acid/rpc/rpc_connection_pool.h"
#include "acid/rpc/serializer.h"

void test_call() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8000");
    acid::rpc::RpcConnectionPool::ptr con = std::make_shared<acid::rpc::RpcConnectionPool>();
    con->connect(address);

    std::vector<std::string> vec{"a-","b-","c"};
    con->callback("CatString", vec, [](acid::rpc::Result<std::string> str){
        SPDLOG_INFO(str.toString());
    });
    con->callback("CatString", vec, [](acid::rpc::Result<std::string> str){
        SPDLOG_INFO(str.toString());
    });

    auto chan = con->async_call<std::string>("CatString", vec);
    acid::rpc::Result<std::string> str;
    chan >> str;
    SPDLOG_INFO(str.getVal());
    sleep(4);
    int n=0;
    while (n!=1000) {
        n++;
        con->callback("add", 0, n, [](acid::rpc::Result<int> res){
            SPDLOG_INFO(res.toString());
        });
    }
    sleep(10);
    co_sched.Stop();
}

void test_subscribe() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8000");
    acid::rpc::RpcConnectionPool::ptr con = std::make_shared<acid::rpc::RpcConnectionPool>();
    con->connect(address);
    con->subscribe("data",[](acid::rpc::Serializer s){
        int n;
        s >> n;
        std::string str;
        SPDLOG_INFO("recv registry publish: {}", n);
    });
    while (true) {
        sleep(5);
    }
}

void test_subscribe_service() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8000");
    acid::rpc::RpcConnectionPool::ptr con = std::make_shared<acid::rpc::RpcConnectionPool>();
    con->connect(address);

    auto aa = con->call<int>("add", 1, 1);
    SPDLOG_INFO(aa.toString());

    while (true) {
        // 将 test_rpc_server 断开，会看到控制台打印 service [ add : 127.0.0.1:9000 ] quit
        // 将 test_rpc_server 重新连接，会看到控制台打印 service [ add : 127.0.0.1:9000 ] join
        // 实时的发布/订阅模式实现自动维护服务列表
        sleep(1);
    }
}

int main() {
    //go test_call;
    //go test_subscribe;
    go test_subscribe_service;
    co_sched.Start(0);
}