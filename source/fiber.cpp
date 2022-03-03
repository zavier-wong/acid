//
// Created by zavier on 2021/11/21.
//
#include <atomic>
#include "acid/config.h"
#include "acid/fiber.h"
#include "acid/macro.h"
namespace acid{
static Logger::ptr g_logger = ACID_LOG_NAME("system");

static std::atomic<uint64_t> s_fiber_id{0};
static std::atomic<uint64_t> s_fiber_count{0};

//当前协程指针
static thread_local Fiber* t_fiber = nullptr;
//当前线程的主协程指针
static thread_local Fiber::ptr t_threadFiber = nullptr;

static ConfigVar<uint32_t>::ptr g_fiber_stack_size =
        Config::Lookup<uint32_t>("fiber.stack_size",128 * 1024,"Fiber stack size");

class MallocStackAllocator{
public:
    static void* Alloc(size_t size){
        return malloc(size);
    }
    static void Dealloc(void* vp, size_t size){
        free(vp);
    }
};

using StackAllocator = MallocStackAllocator;

// 主协程构造
Fiber::Fiber() {
    m_state = EXEC;
    SetThis(this);
    if(getcontext(&m_ctx)){
        ACID_ASSERT2(false,"System error: getcontext fail");
    }
    ++s_fiber_count;

    //ACID_LOG_DEBUG(g_logger) << "Fiber::Fiber main";
}

//普通协程构造
Fiber::Fiber(std::function<void()> cb, size_t stacksize)
        : m_id(++s_fiber_id)
        , m_cb(cb){
    ACID_ASSERT2(t_fiber,"Fiber error: no main fiber");
    s_fiber_count++;
    m_stacksize = stacksize? stacksize: g_fiber_stack_size->getValue();
    m_stack = StackAllocator::Alloc(m_stacksize);

    if(getcontext(&m_ctx)){
        ACID_ASSERT2(false,"System error: getcontext fail");
    }
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_size = m_stacksize;
    m_ctx.uc_stack.ss_sp = m_stack;

    makecontext(&m_ctx,MainFunc,0);

    //ACID_LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << m_id;
}

Fiber::~Fiber() {
    --s_fiber_count;
    if(m_stack){
        ACID_ASSERT(m_state == INIT || m_state == TERM || m_state == EXCEPT);
        StackAllocator::Dealloc(m_stack, m_stacksize);
    } else {
        ACID_ASSERT(m_state == EXEC);
        ACID_ASSERT(!m_cb);
        if(t_fiber == this){
            SetThis(nullptr);
        }
    }
//    ACID_LOG_DEBUG(g_logger) << "Fiber::~Fiber id=" << m_id
//                              << " total=" << s_fiber_count;
}

void Fiber::reset(std::function<void()> cb) {
    ACID_ASSERT(m_stack);
    ACID_ASSERT(m_state == INIT || m_state == TERM || m_state == EXCEPT);

    m_cb = cb;

    if(getcontext(&m_ctx)){
        ACID_ASSERT2(false,"System error: getcontext fail");
    }
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_size = m_stacksize;
    m_ctx.uc_stack.ss_sp = m_stack;

    makecontext(&m_ctx,MainFunc,0);
    m_state = INIT;
}

void Fiber::resume() {
    SetThis(this);
    ACID_ASSERT2(m_state != EXEC, "Fiber id=" + std::to_string(m_id));

    m_state = EXEC;

    if(swapcontext(&t_threadFiber->m_ctx,&m_ctx)){
        ACID_ASSERT2(false,"system error: swapcontext() fail");
    }
}

void Fiber::yield() {
    SetThis(t_threadFiber.get());
    if(swapcontext(&m_ctx,&t_threadFiber->m_ctx)) {
        ACID_ASSERT2(false,"system error: swapcontext() fail");
    }
}

Fiber::ptr Fiber::GetThis() {
    return t_fiber->shared_from_this();
}

void Fiber::SetThis(Fiber* f) {
    t_fiber = f;
}

void Fiber::YieldToHold() {
    Fiber::ptr cur = GetThis();
    cur->m_state = HOLD;
    cur->yield();
}

void Fiber::YieldToReady() {
    Fiber::ptr cur = GetThis();
    cur->m_state = READY;
    cur->yield();
}

uint64_t Fiber::TotalFibers() {
    return s_fiber_count;
}
void Fiber::MainFunc(){
    Fiber::ptr cur = GetThis();
    ACID_ASSERT(cur);
    try {
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    }catch (std::exception& ex){
        cur->m_state = EXCEPT;
        ACID_LOG_ERROR(g_logger) << "Fiber Except: " <<ex.what();
    }catch (...){
        cur->m_state = EXCEPT;
        ACID_LOG_ERROR(g_logger) << "Fiber Except: " ;
    }
    auto ptr = cur.get();
    cur = nullptr;
    ptr->yield();
}

uint64_t Fiber::GetFiberId() {
    if(t_fiber){
        return t_fiber->getId();
    }
    return 0;
}

void Fiber::EnableFiber(){
    if(!t_fiber){
        Fiber::ptr main_fiber(new Fiber);
        ACID_ASSERT(t_fiber == main_fiber.get());
        t_threadFiber = main_fiber;
    }
}


}
