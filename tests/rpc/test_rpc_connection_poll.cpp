//
// Created by zavier on 2022/1/16.
//

#include "acid/rpc/rpc_connection_pool.h"
#include "acid/rpc/serializer.h"
#include "acid/log.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();

void test_discover() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8070");
    //acid::rpc::RpcConnectionPool::ptr con = std::make_shared<acid::rpc::RpcConnectionPool>(5);
    acid::rpc::RpcConnectionPool::ptr con = std::make_shared<acid::rpc::RpcConnectionPool>();
    con->connect(address);

//    auto aa = con->call<int>("add",1,1);
//    ACID_LOG_INFO(g_logger) << aa.toString();
//    aa = con->call<int>("add",2,2);
//    ACID_LOG_INFO(g_logger) << aa.toString();
//    aa = con->call<int>("add",3,3);
//    ACID_LOG_INFO(g_logger) << aa.toString();
    //std::future<acid::rpc::Result<std::string>> b = con->async_call<std::string>("getStr");

    std::vector<std::string> vec{"a-","b-","c"};
    con->async_call<std::string>([](acid::rpc::Result<std::string> str){
        ACID_LOG_INFO(g_logger) << str.toString();
    }, "CatString",vec);
    con->async_call<std::string>([](acid::rpc::Result<std::string> str){
        ACID_LOG_INFO(g_logger) << str.toString();
    }, "CatString",vec);
    //sleep(4);
    int n=0;
    while (n!=10000) {
        ACID_LOG_DEBUG(g_logger) << n++;
            con->async_call<int>([](acid::rpc::Result<int> res){
                ACID_LOG_DEBUG(g_logger) << res.toString();
            },"add",0,n);
//        auto rt = con->call<int>("add",0,n);
//        ACID_LOG_DEBUG(g_logger) << rt.toString();
    }
    //sleep(5);
//    ACID_LOG_INFO(g_logger) << b.get().toString();
//    ACID_LOG_INFO(g_logger) << a.get().toString();
}

void test_subscribe() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8070");
    //acid::rpc::RpcConnectionPool::ptr con = std::make_shared<acid::rpc::RpcConnectionPool>(5);
    acid::rpc::RpcConnectionPool::ptr con = std::make_shared<acid::rpc::RpcConnectionPool>();
    con->connect(address);
    con->subscribe("data",[](acid::rpc::Serializer s){
        std::vector<int> vec;
        s >> vec;
        std::string str;
        std::for_each(vec.begin(), vec.end(),[&str](int i) mutable { str += std::to_string(i);});
        LOG_DEBUG << "recv publish: " << str;
    });
    while (true) {
        sleep(5);
    }
}
int main() {
    go test_subscribe;
}