//
// Created by zavier on 2021/11/21.
//

#ifndef ACID_FIBER_H
#define ACID_FIBER_H
#include <functional>
#include <memory>
#include <ucontext.h>
#include "thread.h"
namespace acid{
/**
 * @brief 协程类，自动保存调用者协程的指针，可在协程里面调用另一个协程
 */
class Fiber: public std::enable_shared_from_this<Fiber> {
public:
    using ptr = std::shared_ptr<Fiber>;
    enum State{
        INIT,
        HOLD,
        EXEC,
        TERM,
        READY,
        EXCEPT
    };
    Fiber(std::function<void()> cb,size_t stacksize = 0);
    ~Fiber();
    //重置协程执行函数，并重置状态
    void reset(std::function<void()> cb);
    //切换到当前协程
    void resume();
    //让出当前协程
    void yield();

    uint64_t getId() const { return m_id;}

    bool isTerminate() const {
        return m_state == TERM || m_state == EXCEPT;
    }

    State getState() const { return m_state;}

    //在当前线程启用协程
    static void EnableFiber();

    //获取当前协程
    static Fiber::ptr GetThis();
    //获取当前协程Id
    static uint64_t GetFiberId();
    //设置当前协程
    static void SetThis(Fiber* f);

    //让出协程，并设置协程状态为Ready
    static void YieldToHold();
    //让出协程，并设置协程状态为Hold
    static void YieldToReady();
    //协程总数
    static uint64_t TotalFibers();

    static void MainFunc();

private:
    Fiber();
private:
    /// 协程 id
    uint64_t m_id = 0;
    /// 协程栈大小
    uint32_t m_stacksize = 0;
    /// 协程状态
    State m_state = INIT;
    /// 协程上下文
    ucontext_t m_ctx;
    /// 协程栈指针
    void* m_stack = nullptr;
    /// 协程运行的函数
    std::function<void()> m_cb;
    /// 调用者协程的指针
    Fiber* m_caller = nullptr;
};

}

#endif //ACID_FIBER_H
