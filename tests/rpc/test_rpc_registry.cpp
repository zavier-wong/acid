//
// Created by zavier on 2022/1/15.
//

#include "acid/rpc/rpc_service_registry.h"

void rpc_service_registry() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8070");
    acid::rpc::RpcServiceRegistry::ptr server(new acid::rpc::RpcServiceRegistry());
    while (!server->bind(address)){
        sleep(1);
    }
    server->start();
}
int main() {
    acid::IOManager ioManager{};
    ioManager.submit(rpc_service_registry);
}