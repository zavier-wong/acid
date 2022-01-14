//
// Created by zavier on 2021/12/18.
//

#include <fstream>
#include "acid/acid.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();
void run(){
    acid::Address::ptr addr = acid::IPAddress::Create("www.baidu.com", 80);
    if(!addr) {
        ACID_LOG_INFO(g_logger) << "get addr error";
        return;
    }

    acid::Socket::ptr sock = acid::Socket::CreateTCP(addr);
    bool rt = sock->connect(addr);
    if(!rt) {
        ACID_LOG_INFO(g_logger) << "connect " << addr->toString() << " failed";
        return;
    }

    acid::http::HttpConnection::ptr conn(new acid::http::HttpConnection(sock));
    acid::http::HttpRequest::ptr req(new acid::http::HttpRequest);
    req->setPath("/index");
    req->setHeader("host", "www.baidu.com");
    ACID_LOG_INFO(g_logger) << "req:" << std::endl
                             << req->toString();
    int n = conn->sendRequest(req);
    ACID_LOG_INFO(g_logger) << n;
    auto res = conn->recvResponse();
    if(!res) {
        ACID_LOG_INFO(g_logger) << "recvResponse " << addr->toString() << " failed";
        return;
    }
    ACID_LOG_INFO(g_logger) << res->toString();

}
void test_request() {
    acid::http::HttpResult::ptr result = acid::http::HttpConnection::DoGet("www.baidu.com");
                                //acid::http::HttpConnection::DoGet("localhost:8080/index.html");
    ACID_LOG_INFO(g_logger) << result->toString();
}
void test_pool(){
    acid::http::HttpConnectionPool::ptr pool(new acid::http::HttpConnectionPool(
            "www.baidu.com", "", 80, false, 10, 1000 * 30, 5));

    acid::IOManager::GetThis()->addTimer(1000, [pool](){
        auto r = pool->doGet("/", -1, {{"connection", "keep-alive"}});
        ACID_LOG_INFO(g_logger) << r->toString();

    }, true);
}
int main(){
    acid::IOManager iom(2);
    iom.submit(test_request);
    //iom.submit(run);
    //iom.submit(test_pool);
    return 0;
}
