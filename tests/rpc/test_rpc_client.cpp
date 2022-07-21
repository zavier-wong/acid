//
// Created by zavier on 2022/1/13.
//

#include "acid/rpc/rpc_client.h"
#include "acid/io_manager.h"
#include "acid/log.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();

void test1() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8080");
    acid::rpc::RpcClient::ptr client(new acid::rpc::RpcClient());

    if (!client->connect(address)) {
        ACID_LOG_DEBUG(g_logger) << address->toString();
        return;
    }
    int n=0;
    //std::vector<std::future<acid::rpc::Result<int>>> vec;
    while (n!=1000) {
        //ACID_LOG_DEBUG(g_logger) << n++;
        n++;
        client->async_call<void>([](acid::rpc::Result<> res) {
            ACID_LOG_DEBUG(g_logger) << res.toString();
        }, "sleep");
    }
    auto rt = client->call<int>("add", 0, n);
    ACID_LOG_DEBUG(g_logger) << rt.toString();
    //sleep(3);
    //client->close();
    client->setTimeout(1000);
    auto sl = client->call<void>("sleep");
    ACID_LOG_DEBUG(g_logger) << "sleep 2s " << sl.toString();

}
void subscribe() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8080");
    acid::rpc::RpcClient::ptr client(new acid::rpc::RpcClient());

    if (!client->connect(address)) {
        ACID_LOG_DEBUG(g_logger) << address->toString();
        return;
    }
    client->subscribe("iloveyou",[](acid::rpc::Serializer s){
        std::string str;
        s >> str;
        LOG_DEBUG << str;
    }) ;
    while(true)
    sleep(100);
}
// 测试重连
void test_retry() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8080");
    acid::rpc::RpcClient::ptr client(new acid::rpc::RpcClient());

    if (!client->connect(address)) {
        ACID_LOG_DEBUG(g_logger) << address->toString();
        return;
    }
    client->close();
    //client = std::make_shared<acid::rpc::RpcClient>();

    if (!client->connect(address)) {
        ACID_LOG_DEBUG(g_logger) << address->toString();
        return;
    }
    int n=0;
    //std::vector<std::future<acid::rpc::Result<int>>> vec;
    while (n!=1000) {
        sleep(2);
        //ACID_LOG_DEBUG(g_logger) << n++;
        n++;
        auto res = client->call<int>("add",1,n);
        if (res.getCode() == acid::rpc::RpcState::RPC_CLOSED) {
            //client = std::make_shared<acid::rpc::RpcClient>();
            if (!client->connect(address)) {
                ACID_LOG_DEBUG(g_logger) << address->toString();
            }
            res = client->call<int>("add",1,n);
        }
        LOG_DEBUG << res.toString();

    }
}
int main() {
    //go test1;
    //go subscribe;
    go test_retry;
}
