//
// Created by zavier on 2021/11/25.
//
#include <algorithm>
#include <signal.h>
#include "acid/hook.h"
#include "acid/scheduler.h"
#include "acid/macro.h"
#include "acid/log.h"
#include "acid/util.h"
namespace acid{
static Logger::ptr g_logger = ACID_LOG_NAME("system");
static thread_local Scheduler* t_scheduler = nullptr;


Scheduler::Scheduler(size_t threads, const std::string &name)
        :m_name(name), m_threadCount(threads){
    t_scheduler = this;
}
Scheduler::~Scheduler() {
    ACID_ASSERT(m_stop);
    if(GetThis() == this){
        t_scheduler = nullptr;
    }
}
void Scheduler::start() {
    MutexType::Lock lock(m_mutex);
    //调度器没有停止就直接返回
    if (m_stop == false){
        return;
    }
    m_stop = false;
    ACID_ASSERT(m_threads.empty());
    m_threads.resize(m_threadCount);
    m_threadIds.resize(m_threadCount);
    for(size_t i = 0; i < m_threadCount; ++i){
        m_threads[i].reset(new acid::Thread(m_name + '_' + std::to_string(i),[this]{
            this->run();
        }));
        m_threadIds[i] = m_threads[i]->getId();
    }

}

void Scheduler::stop() {
    m_stop = true;
    for(size_t i = 0; i < m_threadCount; i++){
        notify();
    }
    std::vector<Thread::ptr> vec;
    vec.swap(m_threads);
    for(auto& t: vec){
        t->join();
    }
}

Scheduler *Scheduler::GetThis() {
    return t_scheduler;
}

void Scheduler::run() {
    signal(SIGPIPE, SIG_IGN);
    setThis();
    acid::Fiber::EnableFiber();
    acid::set_hook_enable(true);
    Fiber::ptr cb_fiber;
    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::wait, this)));

    ScheduleTask task;
    while (true){
        task.reset();
        bool tickle = false;
        //线程取出任务
        {
            MutexType::Lock lock(m_mutex);
            auto it = m_tasks.begin();
            while (it != m_tasks.end()){
                if(it->thread != -1 && GetThreadId() != it->thread){
                    ++it;
                    tickle = true;
                    continue;
                }
                ACID_ASSERT(*it);
                if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
                    ++it;
                    continue;
                }
                task = *it;
                m_tasks.erase(it++);
                break;
            }
            if(it != m_tasks.end()){
                tickle = true;
            }
        }

        if(tickle){
            notify();
        }

        if(task.fiber&& (task.fiber->getState() != Fiber::TERM
                         && task.fiber->getState() != Fiber::EXCEPT)) {
            ++m_activeThreads;
            task.fiber->resume();
            --m_activeThreads;
            if(task.fiber->getState() == Fiber::READY){
                submit(task.fiber);
            }
            task.reset();
        } else if(task.cb){
            if(cb_fiber){
                cb_fiber->reset(task.cb);
            }else{
                cb_fiber.reset(new Fiber(task.cb));
            }
            task.reset();
            ++m_activeThreads;
            cb_fiber->resume();
            --m_activeThreads;
            if(cb_fiber->getState() == Fiber::READY){
                submit(cb_fiber);
                cb_fiber.reset();
            } else if (cb_fiber->isTerminate()){
                cb_fiber->reset(nullptr);
            } else {
                cb_fiber = nullptr;
            }
        } else{

            if(idle_fiber->getState() == Fiber::TERM){
                break;
            }
            ++m_idleThreads;
            idle_fiber->resume();
            --m_idleThreads;
        }
    }
}

bool Scheduler::stopping() {
    MutexType::Lock lock(m_mutex);
    return m_stop && m_tasks.empty() && m_activeThreads == 0;
}

void Scheduler::setThis() {
    t_scheduler = this;
}

void Scheduler::notify() {
    ACID_LOG_INFO(g_logger) << "notify";
}

void Scheduler::wait() {
    //m_sem.wait();
    ACID_LOG_INFO(g_logger) << "idle";
    while(!stopping()) {
        acid::Fiber::YieldToHold();
    }
    ACID_LOG_DEBUG(g_logger) << "idle fiber exit";
}

}