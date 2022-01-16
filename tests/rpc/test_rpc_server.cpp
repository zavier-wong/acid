//
// Created by zavier on 2022/1/13.
//

#include "acid/rpc/rpc_server.h"
int add(int a,int b){
    return a + b;
}
std::string getStr() {
    return  "hello world";
}
int main() {

    acid::IOManager::ptr ioManager(new acid::IOManager{4});
    ioManager->submit([]{
        std::string str = "lambda";
        acid::Address::ptr address = acid::Address::LookupAny("0.0.0.0:8080");
        acid::Address::ptr registry = acid::Address::LookupAny("0.0.0.0:8070");
        acid::rpc::RpcServer::ptr server(new acid::rpc::RpcServer());
        server->bindRegistry(registry);
        server->registerMethod("add",add);
        server->registerMethod("getStr",getStr);
        server->registerMethod("bind", std::function<int(int,int)>(add));
        server->registerMethod("sleep",[](){
            sleep(100);
        });
        server->registerMethod("lambda",[str]()->std::string{
            return str;
        });
        while (!server->bind(address)){
            sleep(1);
        }

        server->start();
    });
}