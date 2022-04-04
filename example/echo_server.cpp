//
// Created by zavier on 2021/12/14.
//
#include "acid/byte_array.h"
#include "acid/io_manager.h"
#include "acid/log.h"
#include "acid/net/tcp_server.h"

static acid::Logger::ptr g_logger = ACID_LOG_ROOT();

class EchoServer : public acid::TcpServer {
public:
    void handleClient(acid::Socket::ptr client) override {
        ACID_LOG_INFO(g_logger) << "handleClient" << client->toString();
        acid::ByteArray::ptr buff(new acid::ByteArray);
        while (true) {
            buff->clear();
            std::vector<iovec> iovs;
            buff->getWriteBuffers(iovs, 1024);

            int n = client->recv(&iovs[0], iovs.size());
            //int n = 0;
            if(n == 0) {
                ACID_LOG_INFO(g_logger) << "Client Close" << client->toString();
                break;
            } else if (n < 0){
                ACID_LOG_INFO(g_logger) << "Client Error, errno=" << errno << " errstr=" << strerror(errno);
                break;
            }
            buff->setPosition(buff->getPosition() + n);
            buff->setPosition(0);
            ACID_LOG_INFO(g_logger) << "Client: " << buff->toString();
            //client->send(buff);
        }
    }
};
void run(){
    EchoServer::ptr echoServer(new EchoServer);
    acid::Address::ptr address = acid::Address::LookupAny("127.0.0.1:8080");

    while(!echoServer->bind(address)) {
        sleep(2);
    }
    echoServer->start();
}
int main(){
    go run;
}