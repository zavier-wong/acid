//
// Created by zavier on 2021/11/17.
//
#include "acid/acid.h"
#include <memory>
#include <cstdlib>
int n=0;
acid::RWMutex rwmutex;
acid::Mutex mutex;
void f(){
    ACID_LOG_WARN(ACID_LOG_NAME("system"))
        <<"acid::Thread::GetName()  "<<acid::Thread::GetName()
        <<"acid::Thread::GetThis()->getName()  "<<acid::Thread::GetThis()->getName()
        <<"acid::Thread::GetThis()->getId(); " <<acid::Thread::GetThis()->getId()
        <<"acid::GetThreadId() "<<acid::GetThreadId();
    //acid::ScopedLock<acid::Mutex> a(mutex);
    //acid::RWMutex::ReadLock a(rwmutex);
    //acid::RWMutex::WriteLock a(rwmutex);
    for(int i=0;i<10000000;i++){
        //acid::ScopedLock<acid::Mutex> a(mutex);
        //acid::RWMutex::ReadLock a(rwmutex);
        //acid::RWMutex::WriteLock a(rwmutex);
        n++;
    }
}
void f2(){
    //acid::Thread t;
    acid::TimeMeasure time;
    acid::Thread::ptr thread[10];

    for(int i=0;i<10;i++){
        thread[i]=std::make_shared<acid::Thread>(std::to_string(i)+" t",&f);
    }

    for(int i=0;i<10;i++){
        thread[i]->join();
    }
    std::cout<<n;
}
void p1(){
    for (int i = 0; ; ++i) {
        ACID_LOG_WARN(ACID_LOG_ROOT())<<"++++++++++++++++++++++++++";
    }

};
void p2(){
    for (int i = 0; ; ++i) {
        ACID_LOG_ERROR(ACID_LOG_ROOT())<<"-----------------------------";
    }

};
int main(){
    acid::Thread b("f1",&p1);
    acid::Thread a("f2",&p2);
    a.join();
    b.join();
}