//
// Created by zavier on 2022/11/23.
//
#include <spdlog/spdlog.h>
#include <libgo/libgo.h>
#include "acid/common/util.h"

using namespace acid;

static int n = 0;
void test_cycle_timer() {
    CycleTimer(1000, [] {
        SPDLOG_INFO("{}", n);
        n++;
    }, nullptr, 3);
    sleep(4);
    auto tk = CycleTimer(1000, [] {
        SPDLOG_INFO("cancel {}", n);
        n++;
    });
    go [=]() mutable {
        sleep(5);
        SPDLOG_WARN("stop");
        tk.stop();
        co_sched.Stop();
    };
};

int main() {
    go test_cycle_timer;
    co_sched.Start(0);
}