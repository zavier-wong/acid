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

int main(int argc, char **argv){
    acid::Fiber::EnableFiber();
    // go test1;
}