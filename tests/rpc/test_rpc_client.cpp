//
// Created by zavier on 2022/1/13.
//

#include "acid/rpc/rpc_client.h"
#include "acid/io_manager.h"
#include "acid/log.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();
int main() {
    acid::IOManager::ptr ioManager(new acid::IOManager{4});
    ioManager->submit([]{
        acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8080");
        acid::rpc::RpcClient::ptr client(new acid::rpc::RpcClient());

        if (!client->connect(address)) {
            ACID_LOG_DEBUG(g_logger) << address->toString();
            return;
        }
        client->setTimeout(2000);
        //auto s = client->async_call<void>("sleep");
        //auto res = client->call<int>("add",11111,22222);
        //auto res = client->call<std::string>("getStr");

        auto res = client->call<int>("add", 1 ,2);
        ACID_LOG_DEBUG(g_logger) << res.toString();
        res = client->call<int>("addaa", 1 ,2);
        ACID_LOG_DEBUG(g_logger) << res.toString();
//        auto res = client->async_call<int>("add",11111,22222);
//        ACID_LOG_DEBUG(g_logger) << res.get().toString();
        //s.wait();
    });
}
