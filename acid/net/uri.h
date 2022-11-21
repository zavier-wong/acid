//
// Created by zavier on 2021/12/20.
//

#ifndef ACID_URI_H
#define ACID_URI_H
#include <coroutine>
#include <memory>
#include <ostream>
#include "address.h"

namespace acid {
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
            m_val = std::move(val);
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

/**
 * @brief URI类，支持大部分协议包括 HTTP，HTTPS，FTP，FILE，磁力链接等等
 */
class Uri{
public:
    using ptr = std::shared_ptr<Uri>;
    Uri();

    /**
     * @brief 创建Uri对象
     * @param uri uri字符串
     * @return 解析成功返回Uri对象否则返回nullptr
     */
    static Uri::ptr Create(const std::string& uri);

    Address::ptr createAddress();
    const std::string& getScheme() const { return m_scheme;}
    const std::string& getUserinfo() const { return m_userinfo;}
    const std::string& getHost() const { return m_host;}
    const std::string& getPath() const;
    const std::string& getQuery() const { return m_query;}
    const std::string& getFragment() const { return m_fragment;}
    uint32_t getPort() const;

    void setScheme(const std::string& scheme) { m_scheme = scheme;}
    void setUserinfo(const std::string& userinfo) { m_userinfo = userinfo;}
    void setHost(const std::string& host) { m_host = host;}
    void setPath(const std::string& path) { m_path = path;}
    void setQuery(const std::string& query) { m_query = query;}
    void setFragment(const std::string& fragment) { m_fragment = fragment;}

    std::ostream& dump(std::ostream& ostream);
    std::string toString();

private:
    Task<bool> parse();
    bool isDefaultPort() const;
private:
    std::string m_scheme;
    std::string m_userinfo;
    std::string m_host;
    std::string m_path;
    std::string m_query;
    std::string m_fragment;
    uint32_t m_port;
    const char* m_cur;
    bool m_error{};
};

}
#endif //ACID_URI_H
