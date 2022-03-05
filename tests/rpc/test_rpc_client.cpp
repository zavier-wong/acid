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
        //client->setTimeout(1000);
        //auto s = client->async_call<void>("sleep");
        //auto res = client->call<int>("add",11111,22222);
        //auto res = client->call<std::string>("getStr");

        //auto res = client->call<int>("add", 1 ,2);
        //ACID_LOG_DEBUG(g_logger) << res.toString();
//        auto res = client->call<int>("addaa", 1 ,2);
//        ACID_LOG_DEBUG(g_logger) << res.toString();

//        ACID_LOG_DEBUG(g_logger) << "sleep";
//        auto res = client->call<void>("sleep");
//        client->async_call<int>([](acid::rpc::Result<int> res){
//            ACID_LOG_DEBUG(g_logger) << res.toString();
//        },"add",1,2);
//        client->async_call<void>([](acid::rpc::Result<> res){
//            ACID_LOG_DEBUG(g_logger) << res.toString();
//        },"sleep");

        //ACID_LOG_DEBUG(g_logger) << res.toString();


//        client->async_call([](acid::rpc::Result<int> res){
//            ACID_LOG_DEBUG(g_logger) << res.toString();
//        },"add",2,3);
//        auto rt = client->async_call<int>("add",1,4);
//        ACID_LOG_DEBUG(g_logger) << rt.get().toString();
        int n=0;
        //std::vector<std::future<acid::rpc::Result<int>>> vec;
        while (n!=1000) {
            //ACID_LOG_DEBUG(g_logger) << n++;
            n++;
            client->async_call<void>([](acid::rpc::Result<> res){
                ACID_LOG_DEBUG(g_logger) << res.toString();
            },"sleep");
            //usleep(1000);
//            client->async_call<int>([](acid::rpc::Result<int> res){
//                ACID_LOG_DEBUG(g_logger) << res.toString();
//            },"add",0,n);
//            auto s = client->async_call<int>("add",0,n);
//            ACID_LOG_DEBUG(g_logger) << s.get().toString();
            //auto rt = client->call<int>("add",0,n);
//            vec.push_back(std::move(s));
            //ACID_LOG_DEBUG(g_logger) << rt.toString();
        }
//        for(auto& i:vec) {
//            ACID_LOG_DEBUG(g_logger) << i.get().toString();
//        }
//        client->async_call<void>([](acid::rpc::Result<> res){
//            ACID_LOG_DEBUG(g_logger) << res.toString();
//        },"sleep");
//        client->async_call<void>([](acid::rpc::Result<> res){
//            ACID_LOG_DEBUG(g_logger) << res.toString();
//        },"sleep");

//        auto res = client->async_call<int>("add",11111,22222);
//        ACID_LOG_DEBUG(g_logger) << res.get().toString();
        //s.wait();
        auto rt = client->call<int>("add",0,n);
        ACID_LOG_DEBUG(g_logger) << rt.toString();
        //sleep(3);
        //client->close();
        client->setTimeout(1000);
        auto sl = client->call<void>("sleep");
        ACID_LOG_DEBUG(g_logger) << "sleep 2s " << sl.toString();
        //sleep(100);
        //client->close();
    });
}
