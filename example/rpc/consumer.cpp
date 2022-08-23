//
// Created by zavier on 2022/1/18.
//

#include "acid/log.h"
#include "acid/rpc/rpc_connection_pool.h"

static acid::Logger::ptr g_logger = ACID_LOG_ROOT();

// 连接服务中心，自动服务发现，执行负载均衡决策，同时会缓存发现的结果
void Main() {
    acid::Address::ptr registry = acid::Address::LookupAny("127.0.0.1:8080");

    // 设置连接池的数量
    acid::rpc::RpcConnectionPool::ptr con = std::make_shared<acid::rpc::RpcConnectionPool>();

    // 连接服务中心
    con->connect(registry);

    acid::rpc::Result<int> res;

    // 第一种调用接口，以同步的方式异步调用，原理是阻塞读时会在协程池里调度
    res = con->call<int>("add", 123, 321);
    ACID_LOG_INFO(g_logger) << res.getVal();

    // 第二种调用接口，调用时会立即返回一个channel
    auto chan = con->async_call<int>("add", 123, 321);
    chan >> res;
    ACID_LOG_INFO(g_logger) << res.getVal();

    // 第三种调用接口，异步回调
    con->callback("add", 114514, 114514, [](acid::rpc::Result<int> res){
        ACID_LOG_INFO(g_logger) << res.getVal();
    });

    // 测试并发
    int n=0;
    while(n != 10000) {
        n++;
        con->callback("add", 0, n, [](acid::rpc::Result<int> res){
            ACID_LOG_INFO(g_logger) << res.getVal();
        });
    }
    // 异步接口必须保证在得到结果之前程序不能退出
    sleep(3);
}

int main() {
    go Main;
}