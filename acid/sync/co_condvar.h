//
// Created by zavier on 2022/3/2.
//

#ifndef ACID_CO_CONDVAR_H
#define ACID_CO_CONDVAR_H

#include "mutex.h"

namespace acid {
/**
 * @brief 协程条件变量
 */
class Fiber;
class Timer;
class CoCondVar : Noncopyable {
public:
    using MutexType = SpinLock;
    /**
     * @brief 唤醒一个等待的协程
     */
    void notify();
    /**
     * @brief 唤醒全部等待的协程
     */
    void notifyAll();
    /**
     * @brief 不加锁地等待唤醒
     */
    void wait();
    /**
     * @brief 等待唤醒
     */
    void wait(CoMutex::Lock& lock);

private:
    // 协程等待队列
    std::queue<std::shared_ptr<Fiber>> m_waitQueue;
    // 保护协程等待队列
    MutexType m_mutex;
    // 空任务的定时器，让调度器保持调度
    std::shared_ptr<Timer> m_timer;
};

}
#endif //ACID_CO_CONDVAR_H
