//
// Created by zavier on 2021/11/28.
//

#ifndef ACID_IO_MANAGER_H
#define ACID_IO_MANAGER_H
#include <atomic>
#include <memory>
#include "mutex.h"
#include "scheduler.h"
#include "timer.h"
namespace acid{
class IOManager: public Scheduler, public TimeManager{
public:
    using ptr = std::shared_ptr<IOManager>;
    using RWMutexType = RWMutex;
    enum Event{
        NONE    = 0x0,
        READ    = 0x1,      // EPOLLIN
        WRITE   = 0x4       // EPOLLOUT
    };

private:
    struct FdContext{
        using MutexType = Mutex;
        struct EventContext{
            Scheduler* scheduler = nullptr; //事件执行的Scheduler
            Fiber::ptr fiber;               //事件的协程
            std::function<void()> cb;       //事件的回调函数
            bool empty(){
                return !scheduler && !fiber && !cb;
            }
        };
        EventContext& getContext(Event event);
        void resetContext(EventContext& event);
        void triggerEvent(Event event);
        int fd = 0;             //事件关联的句柄
        EventContext read;      //读事件
        EventContext write;     //写事件
        Event events = NONE;     //注册的事件类型
        MutexType mutex;
    };
public:
    IOManager(size_t threads = 4, const std::string& name = "");

    ~IOManager();

    bool addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    bool delEvent(int fd, Event event);
    bool cancelEvent(int fd, Event event);
    bool cancelAllEvent(int fd);

    static IOManager* GetThis() ;
protected:
    void notify() override;
    void wait() override;
    bool stopping() override;
    void contextResize(size_t size);
    void onInsertAtFront() override;
private:
    /// epoll 文件句柄
    int m_epfd = 0;
    /// pipe 文件句柄，fd[0]读端，fd[1]写端
    int m_tickleFds[2];
    /// 当前等待执行的IO事件数量
    std::atomic<size_t> m_pendingEventCount = {0};
    /// IOManager的Mutex
    RWMutexType m_mutex;
    /// socket事件上下文的容器
    std::vector<FdContext *> m_fdContexts;

    bool stopping(uint64_t &timeout);
};

}
#endif //ACID_IO_MANAGER_H
