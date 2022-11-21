//
// Created by zavier on 2022/1/15.
//
#include <spdlog/spdlog.h>
#include "acid/rpc/route_strategy.h"

std::vector<int> list{1, 2, 3, 4, 5};

void test_random() {
    acid::rpc::RouteStrategy<int>::ptr strategy =
            acid::rpc::RouteEngine<int>::queryStrategy(acid::rpc::Strategy::Random);
    SPDLOG_INFO("random");
    for ([[maybe_unused]] auto i: list) {
        auto a = strategy->select(list);
        SPDLOG_INFO(a);
    }
}

void test_poll() {
    acid::rpc::RouteStrategy<int>::ptr strategy =
            acid::rpc::RouteEngine<int>::queryStrategy(acid::rpc::Strategy::Polling);
    SPDLOG_INFO("Poll");
    for ([[maybe_unused]] auto i: list) {
        auto a = strategy->select(list);
        SPDLOG_INFO(a);
    }
}
void test_hash() {
    acid::rpc::RouteStrategy<int>::ptr strategy =
            acid::rpc::RouteEngine<int>::queryStrategy(acid::rpc::Strategy::HashIP);
    SPDLOG_INFO("Hash");
    for ([[maybe_unused]] auto i: list) {
        auto a = strategy->select(list);
        SPDLOG_INFO(a);
    }
}
int main() {
    test_random();
    test_poll();
    test_hash();
}