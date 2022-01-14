//
// Created by zavier on 2022/1/9.
//

#ifndef ACID_COROUTINE_H
#define ACID_COROUTINE_H
#include <coroutine>

namespace acid{
template<class T>
struct Task {
    struct promise_type;
    using handle = std::coroutine_handle<promise_type>;
    struct promise_type {
        Task get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() {
            return {};
        }
        std::suspend_always final_suspend() noexcept {
            return {};
        }
        void return_value(T val) {
            m_val = val;
        }
        std::suspend_always yield_value(T val) {
            m_val = val;
            return {};
        }
        void unhandled_exception() {
            std::terminate();
        }
        T m_val;
    };
    Task() : m_handle(nullptr) {}
    Task(handle h): m_handle(h){}
    Task(const Task& rhs) = delete;
    Task(Task&& rhs) noexcept
            : m_handle(rhs.m_handle) {
        rhs.m_handle = nullptr;
    }

    Task& operator=(const Task& rhs) = delete;
    Task& operator=(Task&& rhs) noexcept {
        if (std::addressof(rhs) != this)
        {
            if (m_handle)
            {
                m_handle.destroy();
            }

            m_handle = rhs.m_handle;
            rhs.m_handle = nullptr;
        }

        return *this;
    }
    T get() {
        return m_handle.promise().m_val;
    }
    void resume() {
        m_handle.resume();
    }
    bool done() {
        return !m_handle || m_handle.done();
    }
    void destroy() {
        m_handle.destroy();
        m_handle = nullptr;
    }

    ~Task() {
        if (m_handle) {
            m_handle.destroy();
        }
    }
private:
    handle m_handle = nullptr;
};
template<>
struct Task<void> {
    struct promise_type;
    using handle = std::coroutine_handle<promise_type>;
    struct promise_type {
        Task get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() {
            return {};
        }
        std::suspend_always final_suspend() noexcept {
            return {};
        }
        void return_void() {
        }
        std::suspend_always yield_value() {
            return {};
        }
        void unhandled_exception() {
            std::terminate();
        }
    };
    Task() : m_handle(nullptr) {}
    Task(handle h): m_handle(h){}
    Task(const Task& rhs) = delete;
    Task(Task&& rhs) noexcept
            : m_handle(rhs.m_handle) {
        rhs.m_handle = nullptr;
    }

    Task& operator=(const Task& rhs) = delete;
    Task& operator=(Task&& rhs) noexcept {
        if (std::addressof(rhs) != this)
        {
            if (m_handle)
            {
                m_handle.destroy();
            }

            m_handle = rhs.m_handle;
            rhs.m_handle = nullptr;
        }

        return *this;
    }

    void resume() {
        m_handle.resume();
    }
    bool done() {
        return !m_handle || m_handle.done();
    }
    void destroy() {
        m_handle.destroy();
        m_handle = nullptr;
    }

    ~Task() {
        if (m_handle) {
            m_handle.destroy();
        }
    }

    handle m_handle = nullptr;
};
}
#endif //ACID_COROUTINE_H
