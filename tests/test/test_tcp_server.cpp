//
// Created by zavier on 2021/12/14.
//
#include "acid/net/socket_stream.h"
#include "acid/net/tcp_server.h"
using namespace acid;

class MyTcpServer : public TcpServer {
public:
    void handleClient(Socket::ptr client) override {
        std::string buff;
        buff.resize(1024);
        SocketStream::ptr session = std::make_shared<SocketStream>(client);
        while (true) {
            auto n = session->readFixSize(&buff[0], 5);
            if (n <= 0) {
                SPDLOG_WARN("client closed");
                return;
            }
            buff[n] = 0;
            spdlog::info(buff);
        }
    }
};

int main(){
    auto addr = Address::LookupAny("0.0.0.0:8080");
    //auto addr = acid::UnixAddress::ptr(new acid::UnixAddress("/tmp/acid/unix"));
    SPDLOG_INFO(addr->toString());
    MyTcpServer server;
    while(!server.bind(addr)){
        sleep(3);
    }

    std::jthread t([&server]{
        // 3秒后停止server
        sleep(3);
        server.stop();
    });
    server.start();
}
