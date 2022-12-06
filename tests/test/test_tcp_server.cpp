//
// Created by zavier on 2021/12/14.
//
#include "acid/net/socket_stream.h"
#include "acid/net/tcp_server.h"

using namespace acid;

class EchoServer : public TcpServer {
public:
    void handleClient(Socket::ptr client) override {
        SPDLOG_INFO("handleClient {}", client->toString());
        ByteArray::ptr buff(new ByteArray);
        while (true) {
            buff->clear();
            std::vector<iovec> iovs;
            buff->getWriteBuffers(iovs, 1024);
            int n = client->recv(&iovs[0], iovs.size());
            if(n == 0) {
                SPDLOG_INFO("Client Close {}", client->toString());
                break;
            } else if (n < 0){
                SPDLOG_INFO("Client Error, errno={}, errstr={}", errno, strerror(errno));
                break;
            }
            buff->setPosition(buff->getPosition() + n);
            buff->setPosition(0);
            auto str = buff->toString();
            SPDLOG_INFO("Client: {}", str);
            client->send(str.c_str(), str.length());
        }
    }
};

int main(){
    go [] {
        auto addr = Address::LookupAny("0.0.0.0:8080");
        SPDLOG_INFO(addr->toString());
        EchoServer server;
        while(!server.bind(addr)){
            sleep(3);
        }
        server.start();
    };
    co_sched.Start();
}
