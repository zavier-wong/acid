//
// Created by zavier on 2021/12/14.
//

#include "acid/acid.h"

static auto g_logger = ACID_LOG_ROOT();
auto a = new acid::IOManager(4,"workIO");
void run() {
    auto addr = acid::Address::LookupAny("0.0.0.0:8080");
    //auto addr = acid::UnixAddress::ptr(new acid::UnixAddress("/tmp/acid/unix"));
    ACID_LOG_DEBUG(g_logger) << addr->toString();

    //auto b = acid::IOManager::GetThis();
    acid::TcpServer::ptr tcpServer(new acid::TcpServer(a));

    while(!tcpServer->bind(addr)){
        sleep(3);
    }

    tcpServer->start();
    ACID_LOG_DEBUG(g_logger) << "start";
    //sleep(10000);
}
int main(){
    acid::IOManager::ptr ioManager = acid::IOManager::ptr(new acid::IOManager(4,"mainIO"));
    ioManager->submit(&run);
    //sleep(1000);
}
