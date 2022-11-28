//
// Created by zavier on 2022/1/15.
//

#include "acid/rpc/rpc_service_registry.h"

using namespace acid;
using namespace rpc;

void rpc_service_registry() {
    Address::ptr address = Address::LookupAny("127.0.0.1:8000");
    RpcServiceRegistry server;
    while (!server.bind(address)){
        sleep(1);
    }
    server.start();
}
void test_publish() {
    Address::ptr address = Address::LookupAny("127.0.0.1:8000");
    RpcServiceRegistry server;
    while (!server.bind(address)){
        sleep(1);
    }
    go [&] {
        int n = 0;
        //std::vector<int> vec;
        while (true) {
            //vec.push_back(n);
            sleep(3);
            server.publish("data", n);
            ++n;
        }
    };
    server.start();
}
int main() {
    go test_publish;
    co_sched.Start();
}