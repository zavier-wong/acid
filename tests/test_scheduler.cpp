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

void test_fiber_mutex(){
    acid::Mutex m;
    //static int i=0;
    ACID_LOG_INFO(g_logger) << "start lock";
    m.lock();
    ACID_LOG_INFO(g_logger) << "Yield";
    acid::Fiber::YieldToReady();
    ACID_LOG_INFO(g_logger) << "resume";
    m.unlock();
    ACID_LOG_INFO(g_logger) << "unlock";
}

void f2() {
    ACID_LOG_DEBUG(g_logger) << "Im fiber 2";
    acid::Fiber::YieldToReady();
}

void f1() {
    ACID_LOG_DEBUG(g_logger) << "Im fiber 1";
    acid::Fiber::ptr fiber(new acid::Fiber(f2));
    //fiber->setCaller(acid::Fiber::GetThis());
    fiber->resume();
}
int main(int argc, char** argv) {
    ACID_LOG_DEBUG(g_logger) << "main";
    acid::Scheduler sc(3,"test");
    sc.start();
    //sc.submit(&test_fiber_mutex);
    //sc.submit(&test_fiber_mutex);
    sc.submit(&f1);
    return 0;
}