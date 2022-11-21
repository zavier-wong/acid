//
// Created by zavier on 2022/1/18.
//
#include <spdlog/spdlog.h>
#include "acid/rpc/rpc_connection_pool.h"

using namespace acid;
using namespace acid::rpc;

// 连接服务中心，自动服务发现，执行负载均衡决策，同时会缓存发现的结果
void Main() {
    Address::ptr registry = Address::LookupAny("127.0.0.1:8000");

    // 设置连接池的数量
    RpcConnectionPool::ptr con = std::make_shared<RpcConnectionPool>();

    // 连接服务中心
    con->connect(registry);

    Result<int> res;

    // 第一种调用接口，以同步的方式异步调用，原理是阻塞读时会在协程池里调度
    res = con->call<int>("add", 123, 321);
    SPDLOG_INFO(res.getVal());

    // 第二种调用接口，调用时会立即返回一个channel
    auto chan = con->async_call<int>("add", 123, 321);
    chan >> res;
    SPDLOG_INFO(res.getVal());

    // 第三种调用接口，异步回调
    con->callback("add", 114514, 114514, [](Result<int> res){
        SPDLOG_INFO(res.getVal());
    });

    // 测试并发
    int n=0;
    while(n != 10000) {
        n++;
        con->callback("add", 0, n, [](Result<int> res){
            SPDLOG_INFO(res.toString());
        });
    }
    // 异步接口必须保证在得到结果之前程序不能退出
    sleep(10);
    SPDLOG_WARN("stop");
    co_sched.Stop();
}

int main() {
    go Main;
    co_sched.Start(0);
}