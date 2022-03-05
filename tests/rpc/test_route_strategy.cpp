//
// Created by zavier on 2022/1/15.
//

#include "acid/rpc/route_strategy.h"

#include "acid/log.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();
std::vector<int> list{1, 2, 3, 4, 5};

void test_random() {
    acid::rpc::RouteStrategy<int>::ptr strategy =
            acid::rpc::RouteEngine<int>::queryStrategy(acid::rpc::Strategy::Random);

    ACID_LOG_DEBUG(g_logger) << "random";
    for ([[maybe_unused]] auto i: list) {
        auto a = strategy->select(list);
        ACID_LOG_DEBUG(g_logger) << a;
    }
}

void test_poll() {
    acid::rpc::RouteStrategy<int>::ptr strategy =
            acid::rpc::RouteEngine<int>::queryStrategy(acid::rpc::Strategy::Polling);

    ACID_LOG_DEBUG(g_logger) << "Poll";
    for ([[maybe_unused]] auto i: list) {
        auto a = strategy->select(list);
        ACID_LOG_DEBUG(g_logger) << a;
    }
}
void test_hash() {
    acid::rpc::RouteStrategy<int>::ptr strategy =
            acid::rpc::RouteEngine<int>::queryStrategy(acid::rpc::Strategy::HashIP);

    ACID_LOG_DEBUG(g_logger) << "Hash";
    for ([[maybe_unused]] auto i: list) {
        auto a = strategy->select(list);
        ACID_LOG_DEBUG(g_logger) << a;
    }
}
int main() {
    test_random();
    test_poll();
    test_hash();
}