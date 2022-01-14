//
// Created by zavier on 2021/11/22.
//

#include "acid/fiber.h"
#include "acid/log.h"

acid::Logger::ptr g_logger = ACID_LOG_ROOT();
int count = 0;
void run_in_fiber(){
    ACID_LOG_INFO(g_logger)<<"run_in_fiber begin";
    acid::Fiber::YieldToReady();
    ACID_LOG_INFO(g_logger)<<"run_in_fiber end";
}

void test1(){
    //acid::Fiber::EnableFiber();
    ACID_LOG_INFO(g_logger)<<"begin";
    acid::Fiber::ptr fiber(new acid::Fiber(run_in_fiber));
    fiber->resume();
    ACID_LOG_INFO(g_logger)<<"after swap in";
    fiber->resume();
    ACID_LOG_INFO(g_logger)<<"end";
}
void test2(){
    acid::Fiber::ptr fiber(new acid::Fiber([](){
        while (1){
            count++;
            acid::Fiber::YieldToReady();
        }
    }));
    while (1){
        fiber->resume();
        ACID_LOG_DEBUG(g_logger)<<count;
    }

}
void test_called_fiber(){

    ACID_LOG_DEBUG(g_logger) << "I was called";
    //acid::Fiber::YieldToReady();
    ACID_LOG_WARN(g_logger) << "I was called";
}
void test_caller_fiber(){

    acid::Fiber::ptr fiber(new acid::Fiber(&test_called_fiber));
    //fiber->resume();
    fiber->resume();
    ACID_LOG_DEBUG(g_logger) << " Im calling";
}



void f3() {
    ACID_LOG_DEBUG(g_logger) << "Im fiber 3";
    acid::Fiber::YieldToReady();
    ACID_LOG_DEBUG(g_logger) << "Im fiber 3 resume from fiber 2";
}

void f2() {
    ACID_LOG_DEBUG(g_logger) << "Im fiber 2";
    acid::Fiber::ptr fiber(new acid::Fiber(f3));
    fiber->resume();
    fiber->resume();
}

void f1() {
    ACID_LOG_DEBUG(g_logger) << "Im fiber 1";
    acid::Fiber::ptr fiber(new acid::Fiber(f2));
    fiber->resume();
}
int main(int argc, char **argv){
    acid::Fiber::EnableFiber();
    acid::Fiber::ptr fiber(new acid::Fiber(&f1));
    fiber->resume();
    ACID_LOG_DEBUG(g_logger) << "Im main";
}