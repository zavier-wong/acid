//
// Created by zavier on 2021/11/28.
//
#include <unistd.h>
#include "acid/acid.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();

void test_fiber() {
    static int s_count = 5;
    ACID_LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

    sleep(1);
    while(--s_count >= 0) {
        //acid::Fiber::GetThis()->YieldToReady();

        //acid::Fiber::GetThis()->YieldToReady();
        //acid::Scheduler::GetThis()->submit(&test_fiber, acid::GetThreadId());
        acid::Fiber::YieldToReady();
        ACID_LOG_INFO(g_logger) << "resum  " << s_count;
    }
    ACID_LOG_INFO(g_logger) << "test end" << s_count;
}

void test_fiber2(){
    while(1){
        ACID_LOG_INFO(g_logger) << "while " ;
        sleep(2);
        acid::Fiber::YieldToReady();
    }
}

void test3(){
    ACID_LOG_INFO(g_logger) << "main";
    acid::Scheduler sc(3,"test");
    sc.start();
    sleep(2);
    ACID_LOG_INFO(g_logger) << "schedule";
    sc.submit(&test_fiber);
    //sc.submit(acid::Fiber::ptr (new acid::Fiber(&test_fiber)));
    //sc.submit(&test_fiber2);
    //sleep(8);
    //sc.stop();
    while(1);
    ACID_LOG_INFO(g_logger) << "over";
}

int main(int argc, char** argv) {
    ACID_LOG_DEBUG(g_logger) << "main";
    acid::Scheduler sc(3,"test");
    sc.start();
    sc.submit([]{
        ACID_LOG_INFO(g_logger) << "hello world";
    });
    return 0;
}