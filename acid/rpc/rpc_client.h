//
// Created by zavier on 2022/1/13.
//

#ifndef ACID_RPC_CLIENT_H
#define ACID_RPC_CLIENT_H
#include <memory>
#include <functional>
#include <future>
#include "acid/ds/safe_queue.h"
#include "acid/io_manager.h"
#include "acid/net/socket.h"
#include "acid/net/socket_stream.h"
#include "protocol.h"
#include "route_strategy.h"
#include "rpc.h"
#include "rpc_session.h"
namespace acid::rpc {

/**
 * @brief 将被打包为 tuple 的函数参数进行序列化
 */
template<typename... Args>
void package_params(Serializer& s, const std::tuple<Args...>& t) {
    /**
     * @brief 实际的序列化函数，利用折叠表达式展开参数包
     */
    const auto& package = []<typename Tuple, std::size_t... Index>
    (Serializer& s, const Tuple& t, std::index_sequence<Index...>) {
        (s << ... << std::get<Index>(t));
    };
    package(s, t, std::index_sequence_for<Args...>{});
}

/**
 * @brief RPC客户端
 */
class RpcClient {
public:
    using ptr = std::shared_ptr<RpcClient>;
    using MutexType = Mutex;

    RpcClient();

    ~RpcClient();

    void close();

    /**
     * @brief 连接一个RPC服务器
     * @param[in] address 服务器地址
     */
    bool connect(Address::ptr address);

    /**
     * @brief 设置RPC调用超时时间
     * @param[in] address 服务器地址
     */
    void setTimeout(uint64_t timeout_ms);
    /**
     * @brief 有参数的调用
     * @param[in] name 函数名
     * @param[in] ps 可变参
     * @return 返回调用结果
     */
    template<typename R, typename... Params>
    Result<R> call(const std::string& name, Params... ps) {
        using args_type = std::tuple<typename std::decay<Params>::type...>;
        args_type args = std::make_tuple(ps...);

        Serializer::ptr s = std::make_shared<Serializer>();
        (*s) << name;
        package_params(*s, args);
        s->reset();
        return call<R>(s);
    }
    /**
     * @brief 无参数的调用
     * @param[in] name 函数名
     * @return 返回调用结果
     */
    template<typename R>
    Result<R> call(const std::string& name) {
        Serializer::ptr s = std::make_shared<Serializer>();
        (*s) << name;
        s->reset();
        return call<R>(s);
    }

    /**
     * @brief 异步远程过程调用回调模式
     * @param[in] callback 回调函数
     * @param[in] name 函数名
     * @param[in] ps 可变参
     */
    template<typename R, typename... Params>
    void async_call(std::function<void(Result<R>)> callback, const std::string& name, Params&& ... ps) {

        std::function<Result<R>()> task = [name, ps..., this] () -> Result<R> {
            return call<R>(name, ps...);
        };

        acid::IOManager::GetThis()->submit([callback, task]{
            callback(task());
        });
    }

    /**
     * @brief 异步调用
     */
    template<typename R,typename... Params>
    std::future<Result<R>> async_call(const std::string& name, Params&& ... ps) {
        std::function<Result<R>()> task = [name, ps..., this] () -> Result<R> {
            return call<R>(name, ps...);
        };
        auto promise = std::make_shared<std::promise<Result<R>>>();
        acid::IOManager::GetThis()->submit([task, promise]{
            promise->set_value(task());
        });
        return promise->get_future();
    }

    Socket::ptr getSocket() {
        return m_session->getSocket();
    }
private:
    void handleSend();
    void handleRecv();
    void handleMethodResponse(Protocol::ptr response);
    /**
     * @brief 实际调用
     * @param[in] s 序列化完的请求
     * @return 返回调用结果
     */
    template<typename R>
    Result<R> call(Serializer::ptr s) {
        Result<R> val;
        if (!m_session || !m_session->isConnected()) {
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");
            return val;
        }

        Protocol::ptr request;

        std::map<uint32_t, std::pair<Fiber::ptr, Protocol::ptr>>::iterator it;
        Fiber::ptr self = Fiber::GetThis();

        {
            MutexType::Lock lock(m_mutex);
            request = Protocol::Create(Protocol::MsgType::RPC_METHOD_REQUEST, s->toString(),m_sequenceId);
            it = m_responseHandle.emplace(m_sequenceId++, std::make_pair(self, nullptr)).first;
        }

        acid::Timer::ptr timer;
        std::shared_ptr<int> timeCondition(new int{0});
        if( m_timeout != (uint64_t)-1 ){
            std::weak_ptr<int> weakPtr(timeCondition);
            acid::IOManager* ioManager = acid::IOManager::GetThis();
            ioManager->addConditionTimer(m_timeout,[weakPtr, ioManager, self, it, this]() mutable {
                auto t = weakPtr.lock();
                if(!t || *t){
                    return ;
                }

                {
                    MutexType::Lock lock(m_mutex);
                    *t = ETIMEDOUT;
                    if (!m_isClose) {
                        m_responseHandle.erase(it);
                    }
                    ioManager->submit(self);
                }

            },weakPtr);
        }

        m_queue.push(request);
        // 让出协程，等待调用结果
        Fiber::YieldToHold();

        if(timer){
            timer->cancel();
        }
        if(*timeCondition){
            // 超时
            val.setCode(RPC_TIMEOUT);
            val.setMsg("call timeout");
            return val;
        }

        Protocol::ptr response;

        {
            MutexType::Lock lock(m_mutex);
            response = m_isClose ? nullptr : it->second.second;
            if (!m_isClose) {
                m_responseHandle.erase(it);
            }
        }

        if (!response) {
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");
            return val;
        }
        if (response->getContent().empty()) {
            val.setCode(RPC_NO_METHOD);
            val.setMsg("Method not find");
            return val;
        }

        Serializer serializer(response->getContent());
        serializer >> val;

        return val;
    }

private:
    std::atomic_bool m_isClose = true;
    std::atomic_bool m_isHearClose = true;
    /// 超时时间
    uint64_t m_timeout = -1;
    /// 服务器的连接
    RpcSession::ptr m_session;
    /// 序列号
    uint32_t m_sequenceId = 0;
    ///
    MutexType m_mutex;

    MutexType m_sendMutex;
    ///
    std::map<uint32_t, std::pair<Fiber::ptr, Protocol::ptr>> m_responseHandle;

    SafeQueue<Protocol::ptr> m_queue;
    /// service provider心跳定时器
    Timer::ptr m_heartTimer;
};


}
#endif //ACID_RPC_CLIENT_H
