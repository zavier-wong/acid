//
// Created by zavier on 2022/4/4.
//
#include "acid/config.h"
#include "acid/io_manager.h"

using namespace acid;
static Logger::ptr g_logger = ACID_LOG_ROOT();
void normal(){
    LOG_DEBUG << __FUNCTION__ ;
}

void test_channel(){
    Channel<int> chan(1);
    Go {
        for (int i = 0; i < 10; ++i) {
            chan << i;
            LOG_DEBUG << "send: " << i;
        }
        chan.close();
    };

    Go {
        int i;
        while (chan >> i) {
            LOG_DEBUG << "recv: " << i;
        }
    };
}


int main(){
    // 设置默认调度器的线程数量
    // Config::Lookup<size_t>("scheduler.threads")->setValue(1);

    // 普通函数
    go normal;

    // 函数对象
    go []{
        LOG_DEBUG << "Lambda";
    };

    std::string str = "hello";

    // Go 关键字默认按值捕获变量
    // 等价于 go [=]() mutable {}
    Go {
        LOG_DEBUG << str;
    };

    go test_channel;
}