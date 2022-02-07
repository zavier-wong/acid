//
// Created by zavier on 2022/1/16.
//

#include "acid/rpc/rpc_connection_pool.h"
#include "acid/log.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();

void test_discover() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8070");
    //acid::rpc::RpcConnectionPool::ptr con = std::make_shared<acid::rpc::RpcConnectionPool>(5);
    acid::rpc::RpcConnectionPool con{5};
    con.connect(address);

    auto aa = con.call<int>("add",123,321);

    std::future<acid::rpc::Result<std::string>> b = con.async_call<std::string>("getStr");

    std::vector<std::string> vec{"a-","b-","c"};
    con.async_call<std::string>([](acid::rpc::Result<std::string> str){
        ACID_LOG_INFO(g_logger) << str.toString();
    }, "CatString",vec);

    sleep(5);
//    ACID_LOG_INFO(g_logger) << b.get().toString();
//    ACID_LOG_INFO(g_logger) << a.get().toString();
}
int main() {
    acid::IOManager ioManager{};
    ioManager.submit(test_discover);
}