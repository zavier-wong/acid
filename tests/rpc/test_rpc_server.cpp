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
std::string CatString(std::vector<std::string> v){
    std::string res;
    for(auto& s:v){
        res+=s;
    }
    return res;
}
int main(int argc, char** argv) {
    int port = 8080;
    if (argv[1]) {
        port = std::stoi(argv[1]);
    }

    acid::IOManager::ptr ioManager(new acid::IOManager{4});
    ioManager->submit([port]{
        std::string str = "lambda";
        //acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8081");
        acid::Address::ptr address = acid::IPv4Address::Create("127.0.0.1",port);
        acid::Address::ptr registry = acid::Address::LookupAny("127.0.0.1:8070");
        acid::rpc::RpcServer::ptr server(new acid::rpc::RpcServer());
        server->registerMethod("add",add);
        server->registerMethod("getStr",getStr);
        server->registerMethod("CatString", CatString);
        while (!server->bind(address)){
            sleep(1);
        }
        server->bindRegistry(registry);


//        server->registerMethod("bind", std::function<int(int,int)>(add));
//        server->registerMethod("sleep",[](){
//            sleep(100);
//        });
//        server->registerMethod("lambda",[str]()->std::string{
//            return str;
//        });

        server->start();
    });
}