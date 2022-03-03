//
// Created by zavier on 2022/3/2.
//
#include "acid/io_manager.h"
#include "acid/log.h"
#include "acid/sync.h"
#include "acid/time_measure.h"
using namespace acid;
// 测试协程同步原语
static Logger::ptr g_logger = ACID_LOG_ROOT();

using MutexType = CoMutex;
MutexType mutexType;
CoCondVar condVar;
volatile int n = 0;
void a() {
    TimeMeasure timeMesure;
    for (int i = 0; i < 100000; ++i) {
        MutexType::Lock lock(mutexType);
        ++n;
        //ACID_LOG_INFO(g_logger) << n;
    }
    ACID_LOG_INFO(g_logger) << n;
}

void b() {
    for (int i = 0; i < 100000; ++i) {
        MutexType::Lock lock(mutexType);
        ++n;
        //ACID_LOG_INFO(g_logger) << n;
    }
    ACID_LOG_INFO(g_logger) << n;
}

void test_mutex() {
    IOManager loop{};
    loop.submit(a);//->submit(b);
    loop.submit(b);
}

void cond_a() {
    MutexType::Lock lock(mutexType);
    ACID_LOG_INFO(g_logger) << "cond a wait";
    condVar.wait(lock);
    ACID_LOG_INFO(g_logger) << "cond a notify";
}
void cond_b() {
    MutexType::Lock lock(mutexType);
    ACID_LOG_INFO(g_logger) << "cond b wait";
    condVar.wait(lock);
    ACID_LOG_INFO(g_logger) << "cond b notify";
}
void cond_c() {
    sleep(2);
    ACID_LOG_INFO(g_logger) << "notify cone";
    condVar.notify();
    sleep(2);
    ACID_LOG_INFO(g_logger) << "notify cone";
    condVar.notify();
}
void test_condvar() {
    IOManager loop{};
    loop.submit(cond_a);//->submit(b);
    loop.submit(cond_b);
    loop.submit(cond_c);
}
CoSemaphore sem(5);
void sem_a() {
    for (int i = 0; i < 5; ++i) {
        sem.wait();
    }
    ACID_LOG_INFO(g_logger) << "sem_a sleep 2s";
    sleep(2);
    for (int i = 0; i < 5; ++i) {
        sem.notify();
    }
}
void sem_b() {
    ACID_LOG_INFO(g_logger) << "sem_b sleep 1s";
    sleep(1);
    for (int i = 0; i < 5; ++i) {
        sem.wait();
    }
    ACID_LOG_INFO(g_logger) << "sem_b notify";
    for (int i = 0; i < 5; ++i) {
        sem.notify();
    }
}
void test_sem() {
    IOManager loop{};
    loop.submit(sem_a);//->submit(b);
    loop.submit(sem_b);
}

void chan_a(Channel<int> chan) {
    for (int i = 0; i < 10; ++i) {
        chan << i;
        ACID_LOG_INFO(g_logger) << "provider " << i;
    }
    ACID_LOG_INFO(g_logger) << "close";
    chan.close();
}

void chan_b(Channel<int> chan) {
    int i;
    while (chan >> i) {
        ACID_LOG_INFO(g_logger) << "consumer " << i;
    }
    ACID_LOG_INFO(g_logger) << "close";
}
void test_channel() {
    IOManager loop{};
    Channel<int> chan(5);
    loop.submit(std::bind(chan_a, chan));
    loop.submit(std::bind(chan_b, chan));
}
int main() {
    //test_mutex();
    //test_condvar();
    //test_sem();
    test_channel();
}