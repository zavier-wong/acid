//
// Created by zavier on 2022/1/13.
//

#include "acid/rpc/rpc_client.h"
#include "acid/io_manager.h"
#include "acid/log.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();
int main() {
    acid::IOManager::ptr ioManager(new acid::IOManager{1});
    ioManager->submit([]{
        acid::Address::ptr address = acid::Address::LookupAny("localhost:8081");
        acid::rpc::RpcClient::ptr client(new acid::rpc::RpcClient());
        if (!client->connect(address)) {
            ACID_LOG_DEBUG(g_logger) << address->toString();
            return;
        }
        auto res = client->call<int>("add",11111,22222);
        ACID_LOG_DEBUG(g_logger) << res.toString();
    });
}
