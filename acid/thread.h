//
// Created by zavier on 2021/11/17.
//

#ifndef ACID_THREAD_H
#define ACID_THREAD_H
#include <atomic>
#include <string>
#include <memory>
#include <functional>
#include <pthread.h>
#include <unistd.h>
#include "sync.h"
namespace acid{
class Thread{
public:
    using ptr = std::shared_ptr<Thread>;
    using callback = std::function<void()>;
    Thread(const std::string& name, callback cb);
    ~Thread();
    void join();
    const std::string& getName() const { return m_name;}
    pid_t getId() const { return m_id;}

    static void* run(void* arg);
    static const std::string& GetName() ;
    static Thread* GetThis();
    static void SetName(const std::string& name);
private:
    Thread(const Thread& thread) = delete;
    Thread(const Thread&& thread) = delete;
    Thread& operator=(const Thread& thread) = delete;

    pid_t m_id = 0;
    pthread_t m_thread = 0;
    callback m_cb;
    std::string m_name ;
    Semaphore m_sem{1};
};

}
#endif //ACID_THREAD_H
