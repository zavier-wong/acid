//
// Created by zavier on 2021/12/2.
//

#include "acid/acid.h"
#include <iostream>
#include "sys/time.h"
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();
void test5(){
    acid::IOManager ioManager{3};

    acid::Timer::ptr timer1 = ioManager.addTimer(5000,[&timer1]{
        static int i =0;
        i++;
        if(i==3){
            //timer1->reset(2000, true);
            //timer1->refresh();
        }
        ACID_LOG_INFO(g_logger) << i <<" seconds 3";
    }, true);
    acid::Timer::ptr timer = ioManager.addTimer(2000,[&timer,&timer1]{
        static int i =0;
        i++;
        if(i==3){
            //timer->reset(1000, true);
            //timer->cancel();
            //timer1->refresh();
        }
        ACID_LOG_INFO(g_logger) << i <<" seconds 2";
    }, true);
    ioManager.start();
    //sleep(100);

}
acid::Timer::ptr s_timer;
void test1(){
    acid::IOManager ioManager{};
    s_timer = ioManager.addTimer(2,[]{
        static int i =0;
        i++;
        if(i%1000==0){
            //s_timer->reset(1,false);
            s_timer->cancel();
            //t->refresh();
            ACID_LOG_INFO(g_logger) << i <<" seconds";
        }
    }, true);
}
int main(){
    test1();
}