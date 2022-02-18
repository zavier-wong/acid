//
// Created by zavier on 2022/2/16.
//

#ifndef ACID_SAFE_QUEUE_H
#define ACID_SAFE_QUEUE_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

namespace acid {
template<typename T, typename Container = std::queue<T>>
class SafeQueue {
public:
    using MutexType = std::mutex;
    SafeQueue() = default;
    SafeQueue(const SafeQueue&) = delete;
    SafeQueue(SafeQueue&&) = delete;
    SafeQueue& operator=(const SafeQueue&) = delete;
    SafeQueue& operator=(SafeQueue&&) = delete;

    size_t size() {
        std::unique_lock<MutexType> lock(m_mutex);
        return m_queue.size();
    }

    bool empty() {
        std::unique_lock<MutexType> lock(m_mutex);
        return m_queue.empty();
    }

    void push(T& t) {
        std::unique_lock<MutexType> lock(m_mutex);
        m_queue.push(t);
        m_cv.notify_one();
    }

    void push(T&& t) {
        std::unique_lock<MutexType> lock(m_mutex);
        m_queue.push(std::move(t));
        m_cv.notify_one();
    }

    bool tryPop(T& t) {
        std::unique_lock<MutexType> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        t = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    std::shared_ptr<T> tryPop() {
        std::unique_lock<MutexType> lock(m_mutex);
        if (m_queue.empty()) {
            return nullptr;
        }
        std::shared_ptr<T> rt = std::make_shared<T>(std::move(m_queue.front()));
        m_queue.pop();
        return rt;
    }

    void waitAndPop(T& t) {
        std::unique_lock<MutexType> lock(m_mutex);
        m_cv.wait(lock, [this]{
            return !m_queue.empty();
        });

        t = std::move(m_queue.front());
        m_queue.pop();
    }

    std::shared_ptr<T> waitAndPop() {
        std::unique_lock<MutexType> lock(m_mutex);
        m_cv.wait(lock, [this]{
            return !m_queue.empty();
        });
        std::shared_ptr<T> rt = std::make_shared<T>(std::move(m_queue.front()));
        m_queue.pop();
        return rt;
    }

private:
    Container m_queue;
    MutexType m_mutex;
    std::condition_variable m_cv;
};
}
#endif //ACID_SAFE_QUEUE_H
