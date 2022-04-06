//
// Created by zavier on 2022/3/2.
//

#include "acid/fiber.h"
#include "acid/io_manager.h"
#include "acid/sync/co_condvar.h"

namespace acid {
void CoCondVar::notify() {
    Fiber::ptr fiber;
    {
        // 获取一个等待的协程
        MutexType::Lock lock(m_mutex);
        if (m_waitQueue.empty()) {
            return;
        }
        fiber = m_waitQueue.front();
        m_waitQueue.pop();
        if (m_timer) {
            // 删除定时器
            m_timer->cancel();
            m_timer = nullptr;
        }
    }
    // 将协程重新加入调度
    if (fiber) {
        go fiber;
    }
}

void CoCondVar::notifyAll() {
    MutexType::Lock lock(m_mutex);
    // 将全部等待的协程重新加入调度
    while (m_waitQueue.size()) {
        Fiber::ptr fiber = m_waitQueue.front();
        m_waitQueue.pop();
        if (fiber) {
            go fiber;
        }
    }
    // 删除定时器
    if (m_timer) {
        m_timer->cancel();
        m_timer = nullptr;
    }
}

void CoCondVar::wait() {
    Fiber::ptr self = Fiber::GetThis();
    {
        MutexType::Lock lock(m_mutex);
        // 将自己加入等待队列
        m_waitQueue.push(self);
        if (!m_timer) {
            // 加入一个空任务定时器，不让调度器退出
            m_timer = IOManager::GetThis()->addTimer(-1,[]{},true);
        }
    }
    // 让出协程
    Fiber::YieldToHold();
}

void CoCondVar::wait(CoMutex::Lock& lock) {
    Fiber::ptr self = Fiber::GetThis();
    {
        MutexType::Lock lock1(m_mutex);
        // 将自己加入等待队列
        m_waitQueue.push(self);
        if (!m_timer) {
            // 加入一个空任务定时器，不让调度器退出
            m_timer = IOManager::GetThis()->addTimer(-1,[]{},true);
        }
        // 先解锁
        lock.unlock();
    }

    // 让出协程
    Fiber::YieldToHold();
    // 重新获取锁
    lock.lock();
}

}