//
// Created by zavier on 2022/1/13.
//

#include "acid/rpc/rpc_server.h"

using namespace acid;
using namespace rpc;

int add(int a,int b){
    return a + b;
}
std::string getStr() {
    return  "hello world";
}
std::string CatString(std::vector<std::string> v){
    std::string res;
    for(auto& s:v){
        res+=s;
    }
    return res;
}
void Main() {
    int port = 9000;

    Address::ptr address = IPv4Address::Create("127.0.0.1",port);
    Address::ptr registry = Address::LookupAny("127.0.0.1:8000");
    RpcServer server;
    std::string str = "lambda";

    server.registerMethod("add",add);
    server.registerMethod("getStr",getStr);
    server.registerMethod("CatString", CatString);
    server.registerMethod("sleep", []{
        sleep(2);
    });
    while (!server.bind(address)){
        sleep(1);
    }
    server.bindRegistry(registry);
    go [&] {
        int n = 0;
        while (true) {
            sleep(3);
            SPDLOG_INFO("server publish data {}", n);
            server.publish("data", n);
            ++n;
        }
    };
    server.start();
}

int main() {
    go Main;
    co_sched.Start();
}