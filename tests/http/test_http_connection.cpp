//
// Created by zavier on 2021/12/18.
//

#include <fstream>
#include "acid/http/http_connection.h"

void run(){
    acid::Address::ptr addr = acid::IPAddress::Create("www.baidu.com", 80);
    if(!addr) {
        SPDLOG_INFO("get addr error");
        return;
    }

    acid::Socket::ptr sock = acid::Socket::CreateTCP(addr);
    bool rt = sock->connect(addr);
    if(!rt) {
        SPDLOG_INFO("connect {} failed", addr->toString());
        return;
    }

    acid::http::HttpConnection::ptr conn(new acid::http::HttpConnection(sock));
    acid::http::HttpRequest::ptr req(new acid::http::HttpRequest);
    req->setPath("/index");
    req->setHeader("host", "www.baidu.com");
    SPDLOG_INFO("req: \n{}", req->toString());
    int n = conn->sendRequest(req);
    SPDLOG_INFO(n);
    auto res = conn->recvResponse();
    if(!res) {
        SPDLOG_INFO("recvResponse {} failed", addr->toString());
        return;
    }
    SPDLOG_INFO(res->toString());

}
void test_request() {
    acid::http::HttpResult::ptr result = acid::http::HttpConnection::DoGet("www.baidu.com");
                                //acid::http::HttpConnection::DoGet("localhost:8080/index.html");
    SPDLOG_INFO(result->toString());
}
void test_pool(){
    acid::http::HttpConnectionPool::ptr pool(new acid::http::HttpConnectionPool(
            "www.baidu.com", "", 80, false, 10, 1000 * 30, 5));
    auto r = pool->doGet("/", -1, {{"connection", "keep-alive"}});
    SPDLOG_INFO(r->toString());
}
int main(){
    run();
    SPDLOG_WARN("==================");
    test_request();
    SPDLOG_WARN("==================");
    test_pool();
    return 0;
}
