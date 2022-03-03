//
// Created by zavier on 2022/3/3.
//

#ifndef ACID_CHANNEL_H
#define ACID_CHANNEL_H

#include <queue>
#include "co_condvar.h"
namespace acid {
/**
 * @brief Channel 的具体实现
 */
template<typename T>
class ChannelImpl : Noncopyable {
public:
    ChannelImpl(size_t capacity)
            : m_isClose(false)
            , m_capacity(capacity){
    }

    ~ChannelImpl() {

    }
    /**
     * @brief 发送数据到 Channel
     * @param[in] t 发送的数据
     * @return 返回调用结果
     */
    bool push(const T& t) {
        CoMutex::Lock lock(m_mutex);
        if (m_isClose) {
            return false;
        }
        // 如果缓冲区已满，等待m_pushCv唤醒
        while (m_queue.size() >= m_capacity) {
            m_pushCv.wait(lock);
            if (m_isClose) {
                return false;
            }
        }
        m_queue.push(t);
        // 唤醒m_popCv
        m_popCv.notify();
        return true;
    }
    /**
     * @brief 从 Channel 读取数据
     * @param[in] t 读取到 t
     * @return 返回调用结果
     */
    bool pop(T& t) {
        CoMutex::Lock lock(m_mutex);
        if (m_isClose) {
            return false;
        }
        // 如果缓冲区为空，等待m_pushCv唤醒
        while (m_queue.empty()) {
            m_popCv.wait(lock);
            if (m_isClose) {
                return false;
            }
        }
        t = m_queue.front();
        m_queue.pop();
        // 唤醒 m_pushCv
        m_pushCv.notify();
        return true;
    }

    ChannelImpl& operator>>(T& t) {
        pop(t);
        return *this;
    }

    ChannelImpl& operator<<(const T& t) {
        push(t);
        return *this;
    }
    /**
     * @brief 关闭 Channel
     */
    void close() {
        CoMutex::Lock lock(m_mutex);
        if (m_isClose) {
            return;
        }
        m_isClose = true;
        // 唤醒等待的协程
        m_pushCv.notify();
        m_popCv.notify();
        std::queue<T> q;
        std::swap(m_queue, q);
    }

    operator bool() {
        return !m_isClose;
    }

    size_t capacity() const {
        return m_capacity;
    }

    size_t size() {
        CoMutex::Lock lock(m_mutex);
        return m_queue.size();
    }

    bool empty() {
        return !size();
    }
private:
    bool m_isClose;
    // Channel 缓冲区大小
    size_t m_capacity;
    // 协程锁和协程条件变量配合使用保护消息队列
    CoMutex m_mutex;
    // 入队条件变量
    CoCondVar m_pushCv;
    // 出队条件变量
    CoCondVar m_popCv;
    // 消息队列
    std::queue<T> m_queue;
};

/**
 * @brief Channel主要是用于协程之间的通信，属于更高级层次的抽象
 * 在类的实现上采用了 PIMPL 设计模式，将具体操作转发给实现类
 * Channel 对象可随意复制，通过智能指针指向同一个 ChannelImpl
 */
template<typename T>
class Channel {
public:
    Channel(size_t capacity) {
        m_channel = std::make_shared<ChannelImpl<T>>(capacity);
    }
    Channel(const Channel& chan) {
        m_channel = chan.m_channel;
    }
    void close() {
        m_channel->close();
    }
    operator bool() const {
        return *m_channel;
    }

    bool push(const T& t) {
        return m_channel->push(t);
    }

    bool pop(T& t) {
        return m_channel->pop(t);
    }

    Channel& operator>>(T& t) {
        (*m_channel) >> t;
        return *this;
    }

    Channel& operator<<(const T& t) {
        (*m_channel) << t;
        return *this;
    }

    size_t capacity() const {
        return m_channel->capacity();
    }

    size_t size() {
        return m_channel->size();
    }

    bool empty() {
        return m_channel->empty();
    }

    bool unique() const {
        return m_channel.unique();
    }
private:
    std::shared_ptr<ChannelImpl<T>> m_channel;
};

}
#endif //ACID_CHANNEL_H
