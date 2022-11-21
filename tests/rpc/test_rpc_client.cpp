//
// Created by zavier on 2022/1/13.
//

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
        //ACID_LOG_DEBUG(g_logger) << n++;
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
void subscribe() {
    Address::ptr address = Address::LookupAny("127.0.0.1:9000");
    RpcClient::ptr client(new RpcClient());

    if (!client->connect(address)) {
        SPDLOG_INFO(address->toString());
        return;
    }
    client->subscribe("data",[](Serializer s){
        int n;
        s >> n;
        SPDLOG_INFO(n);
    });
    while(true) {
        sleep(2);
    }
}
// 测试重连
void test_retry() {
    Address::ptr address = Address::LookupAny("127.0.0.1:9000");
    RpcClient::ptr client(new RpcClient());
    if (!client->connect(address)) {
        SPDLOG_INFO(address->toString());
        return;
    }
    client->close();
    if (!client->connect(address)) {
        SPDLOG_INFO(address->toString());
        return;
    }
    int n=0;
    while (n!=1000) {
        sleep(2);
        n++;
        auto res = client->call<int>("add",1,n);
        if (res.getCode() == RpcState::RPC_CLOSED) {
            if (!client->connect(address)) {
                SPDLOG_INFO(address->toString());
            }
            res = client->call<int>("add",1,n);
        }
        SPDLOG_INFO(res.toString());
    }
    co_sched.Stop();
}
int main() {
    //go test1;
    go subscribe;
    //go test_retry;
    co_sched.Start(0);
}
