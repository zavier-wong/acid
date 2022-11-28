//
// Created by zavier on 2021/12/14.
//
#include "acid/net/socket_stream.h"
#include "acid/net/tcp_server.h"
using namespace acid;

class MyTcpServer : public TcpServer {
public:
    void handleClient(Socket::ptr client) override {
        SPDLOG_INFO(client->toString());
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
            SPDLOG_INFO(buff);
        }
    }
};

int main(){
    go [] {
        auto addr = Address::LookupAny("0.0.0.0:8080");
        //auto addr = acid::UnixAddress::ptr(new acid::UnixAddress("/tmp/acid/unix"));
        SPDLOG_INFO(addr->toString());
        MyTcpServer server;
        while(!server.bind(addr)){
            sleep(3);
        }

        std::jthread t([&server]{
            // 10秒后停止server
            sleep(10);
            SPDLOG_WARN("server stop after 10s");
            server.stop();
            co_sched.Stop();
        });
        server.start();
    };
    co_sched.Start();
}
