//
// Created by zavier on 2021/11/17.
//

#include "acid/thread.h"
#include "acid/util.h"
#include "acid/log.h"
#include "acid/fiber.h"
namespace acid{
static thread_local Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOWN";
static Logger::ptr g_logger = ACID_LOG_NAME("system");


acid::Thread::Thread(const std::string &name, std::function<void()> cb) {
    m_name = name.empty()? "UNKNOWN": name;
    m_cb = std::move(cb);
    int res = pthread_create(&m_thread, nullptr,&run,this);
    if(res){
        ACID_LOG_ERROR(g_logger) << "pthread_create fail with code=" << res << " name=" << name;
        throw std::logic_error("pthread_create error");
    }
    m_sem.wait();
}

Thread::~Thread() {
    if(m_thread){
        pthread_detach(m_thread);
    }
}

void Thread::join() {
    if(m_thread){
        int res = pthread_join(m_thread, nullptr);
        if(res){
            ACID_LOG_ERROR(g_logger) << "pthread_join fail with code=" << res ;
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;
    }
}

void *Thread::run(void *arg) {
    Thread* thread = (Thread*) arg;
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = acid::GetThreadId();
    pthread_setname_np(thread->m_thread,thread->m_name.substr(0,15).c_str());
    callback cb;
    cb.swap(thread->m_cb);
    thread->m_sem.notify();
    cb();
    return nullptr;
}

const std::string &Thread::GetName() {
    //static std::string main = "MainThread";
    if(t_thread){
        return t_thread_name;
    }
    SetName("main");
    return t_thread_name;
}

Thread *Thread::GetThis() {
    return t_thread;
}

void Thread::SetName(const std::string &name) {
    if(t_thread){
        t_thread->m_name = name;
    }
    t_thread_name = name;
}


}
