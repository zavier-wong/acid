//
// Created by zavier on 2022/12/6.
//

#include "acid/raft/persister.h"
#include "acid/kvraft/kvserver.h"
#include "acid/http/http_server.h"
#include "acid/http/servlets/kvstore_servlet.h"

using namespace acid;
using namespace http;
using namespace raft;
using namespace kvraft;

int64_t id;

std::map<int64_t, std::string> kv_servers = {
        {1, "localhost:7001"},
        {2, "localhost:7002"},
        {3, "localhost:7003"},
};

std::map<int64_t, std::string> http_servers = {
        {1, "localhost:10001"},
        {2, "localhost:10002"},
        {3, "localhost:10003"},
};

void Main() {
    Persister::ptr persister = std::make_shared<Persister>(fmt::format("kvhttp-{}", id));
    KVServer::ptr kv = std::make_shared<KVServer>(kv_servers, id, persister);
    Address::ptr address = Address::LookupAny(http_servers[id]);
    http::HttpServer server(true);
    server.getServletDispatch()->addServlet("/kv", std::make_shared<KVStoreServlet>(kv));
    server.getServletDispatch()->addServlet("/info",[](HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) ->uint32_t {
        response->setBody("<h1>This is a kvstore</h1>");
        return 0;
    });
    go [kv] {
        kv->start();
    };
    while (!server.bind(address)){
        sleep(1);
    }
    server.start();
}

// 启动方法
// ./kvhttp_server 1
// ./kvhttp_server 2
// ./kvhttp_server 3

int main(int argc, char** argv) {
    if (argc <= 1) {
        SPDLOG_ERROR("please input kvhttp id");
        id = 1;
    } else {
        SPDLOG_INFO("argv[1] = {}", argv[1]);
        id = std::stoll(argv[1]);
    }
    go Main;
    co_sched.Start();
}
