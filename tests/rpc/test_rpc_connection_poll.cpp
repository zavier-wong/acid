//
// Created by zavier on 2022/1/16.
//

#include "acid/rpc/rpc_connection_poll.h"
#include "acid/log.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();
void test_discover() {
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8070");
    acid::rpc::RpcConnectionPool con{5};
    con.connect(address);
    auto a = con.call<int>("add",123,321);
    ACID_LOG_INFO(g_logger) << a.toString();
    auto b = con.call<std::string>("getStr");
    ACID_LOG_INFO(g_logger) << b.toString();
    auto c = con.call<int>("bind",123,1);
    ACID_LOG_INFO(g_logger) << c.toString();
//    auto d = con.call<void>("sleep");
//    ACID_LOG_INFO(g_logger) << d.toString();
    auto e = con.call<std::string>("lambda");
    ACID_LOG_INFO(g_logger) << e.toString();

}
int main() {
    acid::IOManager ioManager{};
    ioManager.submit(test_discover);
}