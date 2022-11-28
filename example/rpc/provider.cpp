//
// Created by zavier on 2022/1/18.
//

#include "acid/rpc/rpc_server.h"

using namespace acid;
using namespace acid::rpc;

int add(int a,int b){
    return a + b;
}

// 向服务中心注册服务，并处理客户端请求
void Main() {
    Address::ptr local = IPv4Address::Create("127.0.0.1",9000);
    Address::ptr registry = Address::LookupAny("127.0.0.1:8000");

    RpcServer server;

    // 注册服务，支持函数指针和函数对象，支持标准库容器
    server.registerMethod("add",add);
    server.registerMethod("echo", [](std::string str){
        return str;
    });
    server.registerMethod("reverse", [](std::vector<std::string> vec) -> std::vector<std::string>{
        std::reverse(vec.begin(), vec.end());
        return vec;
    });

    // 先绑定本地地址
    while (!server.bind(local)){
        sleep(1);
    }
    // 绑定服务注册中心
    server.bindRegistry(registry);
    // 开始监听并处理服务请求
    server.start();
}

int main() {
    go Main;
    co_sched.Start(0);
}