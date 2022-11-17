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
    go a;
    go b;
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
    go cond_a;
    go cond_b;
    go cond_c;
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
    go sem_a;
    go sem_b;
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
    int i = 0;
    while (chan >> i) {
        ACID_LOG_INFO(g_logger) << "consumer " << i;
    }
    ACID_LOG_INFO(g_logger) << "close";
}
void test_channel() {
    Channel<int> chan(5);
    go [chan] {
        chan_a(chan);
    };
    go [chan] {
        chan_b(chan);
    };
}
void test_countdown() {
    Go {
        CoCountDownLatch cnt(5);
        for (int i = 0; i < 5; ++i) {
            go [i, &cnt] {
                sleep(i);
                LOG_DEBUG << cnt.getCount();
                cnt.countDown();
            };
        }
        go[&cnt] {
            cnt.wait();
            LOG_DEBUG << cnt.getCount();
        };
        go[&cnt] {
            cnt.wait();
            LOG_DEBUG << cnt.getCount();
        };
        cnt.wait();
        LOG_DEBUG << cnt.getCount();
    };
}
void test_cond_wait_for() {
    Go {
        CoMutex::Lock lock(mutexType);
        for (int i = 1; i < 10000; i *= 2) {
            if (condVar.waitFor(lock, i)) {
                ACID_LOG_INFO(g_logger) << "condvar notify";
                return;
            }
            ACID_LOG_INFO(g_logger) << "condvar wait for " << i << " ms";
        }
    };
    Go {
        sleep(5);
        condVar.notify();
    };
}
void test_channel_wait_for() {
    Channel<int> chan(5);
    Go {
        int val{};
        for (int i = 1; i < 10000; i *= 2) {
            if (chan.waitFor(val, i)) {
                ACID_LOG_INFO(g_logger) << "channel val " << val;
                return;
            }
            ACID_LOG_INFO(g_logger) << "channel wait for " << i << " ms";
        }
    };
    Go {
        sleep(5);
        chan << 114514;
    };
}
int main() {
    //test_mutex();
    //test_condvar();
    //test_sem();
    //test_channel();
    //test_countdown();
    //test_cond_wait_for();
    test_channel_wait_for();
}