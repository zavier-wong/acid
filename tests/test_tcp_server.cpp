//
// Created by zavier on 2021/12/14.
//

#include "acid/acid.h"

static auto g_logger = ACID_LOG_ROOT();

void run() {
    auto addr = acid::Address::LookupAny("0.0.0.0:8080");
    //auto addr = acid::UnixAddress::ptr(new acid::UnixAddress("/tmp/acid/unix"));
    ACID_LOG_DEBUG(g_logger) << addr->toString();

    acid::TcpServer::ptr tcpServer(new acid::TcpServer());

    while(!tcpServer->bind(addr)){
        sleep(3);
    }

    tcpServer->start();
    ACID_LOG_DEBUG(g_logger) << "start";
    //sleep(10000);
}
int main(){
    go run;
}
