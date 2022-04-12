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
void test_publish() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8070");
    acid::rpc::RpcServiceRegistry::ptr server(new acid::rpc::RpcServiceRegistry());
    while (!server->bind(address)){
        sleep(1);
    }
    server->start();
    Go {
        int n = 0;
        std::vector<int> vec;
        while (true) {
            vec.push_back(n);
            sleep(3);
            server->publish("data", vec);
            ++n;
        }
    };
}
int main() {
    go test_publish;
}