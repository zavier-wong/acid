//
// Created by zavier on 2021/11/25.
//

#include "acid/fiber.h"
#include "acid/io_manager.h"
#include "acid/sync/mutex.h"

namespace acid{

bool CoMutex::tryLock() {
    return m_mutex.tryLock();
}

void CoMutex::lock() {
    // 如果本协程已经持有锁就退出
    if (Fiber::GetFiberId() == m_fiberId) {
        return;
    }
    // 第一次尝试获取锁
    while (!tryLock()) {
        // 加锁保护等待队列
        m_gaurd.lock();
        // 由于进入等待队列和出队的代价比较大，所以再次尝试获取锁，成功获取锁将m_fiberId改成自己的id
        if(tryLock()){
            m_gaurd.unlock();
            m_fiberId = Fiber::GetFiberId();
            return;
        }
        // 获取所在的协程
        Fiber::ptr self = Fiber::GetThis();
        // 将自己加入协程等待队列
        m_waitQueue.push(self);
        m_gaurd.unlock();
        // 让出协程
        Fiber::YieldToHold();
    }
    // 成功获取锁将m_fiberId改成自己的id
    m_fiberId = GetFiberId();
}

void CoMutex::unlock() {
    m_gaurd.lock();
    m_fiberId = 0;

    Fiber::ptr fiber;
    if (!m_waitQueue.empty()) {
        // 获取一个等待的协程
        fiber = m_waitQueue.front();
        m_waitQueue.pop();
    }

    m_mutex.unlock();
    m_gaurd.unlock();
    if (fiber) {
        // 将等待的协程重新加入调度
        go fiber;
    }
}

}