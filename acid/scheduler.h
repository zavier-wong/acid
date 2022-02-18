//
// Created by zavier on 2021/11/25.
//

#ifndef ACID_SCHEDULER_H
#define ACID_SCHEDULER_H
#include <atomic>
#include <list>
#include <memory>
#include <string>
#include <queue>
#include <vector>
#include "fiber.h"
#include "mutex.h"
#include "macro.h"
#include "thread.h"
namespace acid{

class Scheduler{
public:
    using ptr = std::shared_ptr<Scheduler>;
    using MutexType = Mutex;
    /**
     * @brief 构造函数
     * @param[in] threads 线程数量
     * @param[in] name 协程调度器名称
     */
    Scheduler(size_t threads = 4, const std::string& name = "");

    virtual ~Scheduler();

    const std::string& getName() const { return m_name;}

    /**
     * @brief 启动协程调度器
     */
    void start();

    /**
     * @brief 停止协程调度器
     */
    void stop();

    /**
     * @brief 提交任务到调度器，任务可以是协程或者可调用对象
     * @param[in] fc 协程或函数
     * @param[in] thread 协程执行的线程id,-1标识任意线程
     */
    template<class FiberOrCb>
    [[maybe_unused]]
    Scheduler* submit(FiberOrCb&& fc, int thread = -1) {
        bool need_notify = false;
        {
            MutexType::Lock lock(m_mutex);
            need_notify = submitNoLock(std::forward<FiberOrCb>(fc), thread);
        }

        if(need_notify) {
            notify();
        }
        return this;
    }
    /**
     * @brief 批量调度协程
     * @param[in] begin 协程数组的开始
     * @param[in] end 协程数组的结束
     */
    template<class InputIterator>
    void submit(InputIterator begin, InputIterator end) {
        bool need_notify = false;
        {
            MutexType::Lock lock(m_mutex);
            while(begin != end) {
                need_notify = submitNoLock(std::move(*begin), -1) || need_notify;
                ++begin;
            }
        }
        if(need_notify) {
            notify();
        }
    }

    static Scheduler* GetThis();
    //static void SetThis(Scheduler* sc);
    //static Fiber* GetMainFiber();
protected:
    /**
     * @brief 通知协程调度器有任务了
     */

    virtual void notify();
    /**
     * @brief 协程调度函数
     */
    void run();

    /**
     * @brief 返回是否可以停止
     */
    virtual bool stopping();

    /**
     * @brief 协程无任务可调度时执行wait协程
     */
    virtual void wait();

    /**
     * @brief 设置当前的协程调度器
     */
    void setThis();

    /**
     * @brief 是否有空闲线程
     */
    bool hasIdleThreads() { return m_idleThreads > 0;}
private:
    struct ScheduleTask{
        Fiber::ptr fiber;
        std::function<void()> cb;
        int thread;
        ScheduleTask(Fiber::ptr& f, int t = -1){
            fiber = f;
            thread = t;
        }
        ScheduleTask(Fiber::ptr&& f, int t = -1){
            fiber = std::move(f);
            thread = t;
            f = nullptr;
        }
        ScheduleTask(std::function<void()>& f, int t = -1){
            cb = f;
            thread = t;
        }
        ScheduleTask(std::function<void()>&& f, int t = -1){
            cb = std::move(f);
            thread = t;
            f = nullptr;
        }
        ScheduleTask(){
            thread = -1;
        }
        void reset(){
            thread = -1;
            fiber = nullptr;
            cb = nullptr;
        }
        operator bool(){
            return fiber || cb;
        }

    };
private:
    /**
     * @brief 协程调度启动(无锁)
     */
    template<class FiberOrCb>
    bool submitNoLock(FiberOrCb&& fc, int thread) {
        bool need_notify = m_tasks.empty();
        ScheduleTask task(std::forward<FiberOrCb>(fc), thread);
        if(task) {
            m_tasks.push_back(task);
        }
        return need_notify;
    }
private:
    //互斥量
    MutexType m_mutex;
    //调度器线程池
    std::vector<Thread::ptr> m_threads;
    //调度器任务
    std::list<ScheduleTask> m_tasks;
    //调度器名字
    std::string m_name;
protected:
    std::vector<int> m_threadIds;
    //线程数量
    size_t m_threadCount = 0;
    //活跃线程数
    std::atomic<size_t> m_activeThreads = 0;
    //空闲线程数
    std::atomic<size_t> m_idleThreads = 0;
    //调度器是否停止
    bool m_stop = true;
};

}
#endif //ACID_SCHEDULER_H
