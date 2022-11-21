//
// Created by zavier on 2022/1/18.
//

#include "acid/rpc/rpc_service_registry.h"

using namespace acid;
using namespace acid::rpc;

// 服务注册中心
int main() {
    Address::ptr address = Address::LookupAny("127.0.0.1:8000");
    RpcServiceRegistry server;
    // 服务注册中心绑定在8080端口
    while (!server.bind(address)){
        sleep(1);
    }
    server.start();
}