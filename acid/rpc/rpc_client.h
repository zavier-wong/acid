//
// Created by zavier on 2022/1/13.
//

#ifndef ACID_RPC_CLIENT_H
#define ACID_RPC_CLIENT_H
#include <memory>
#include <functional>
#include <future>
#include "acid/io_manager.h"
#include "acid/net/socket.h"
#include "acid/net/socket_stream.h"
#include "acid/sync.h"
#include "protocol.h"
#include "route_strategy.h"
#include "rpc.h"
#include "rpc_session.h"
namespace acid::rpc {
/**
 * @brief RPC客户端
 * @details
 * 开启一个 send 协程，通过 Channel 接收调用请求，并转发 request 请求给服务器。
 * 开启一个 recv 协程，负责接收服务器发送的 response 响应并通过序列号获取对应调用者的 Channel，
 * 将 response 放入 Channel 唤醒调用者。
 * 所有调用者的 call 请求将通过 Channel 发送给 send 协程， 然后开启一个 Channel 用于接收 response 响应，
 * 并将请求序列号与自己的 Channel 关联起来。
 */
class RpcClient : public std::enable_shared_from_this<RpcClient> {
public:
    using ptr = std::shared_ptr<RpcClient>;
    using MutexType = CoMutex;

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
        Serializer s;
        s << name << args;
        s.reset();
        return call<R>(s);
    }
    /**
     * @brief 无参数的调用
     * @param[in] name 函数名
     * @return 返回调用结果
     */
    template<typename R>
    Result<R> call(const std::string& name) {
        Serializer s;
        s << name;
        s.reset();
        return call<R>(s);
    }

    /**
     * @brief 异步回调模式
     * @param[in] callback 回调函数
     * @param[in] name 函数名
     * @param[in] ps 可变参
     */
    template<typename R, typename... Params>
    void async_call(std::function<void(Result<R>)> callback, const std::string& name, Params&& ... ps) {
        std::function<Result<R>()> task = [name, ps..., this] () -> Result<R> {
            return call<R>(name, ps...);
        };
        RpcClient::ptr self = shared_from_this();
        go [callback, task, self]() mutable {
            callback(task());
            self = nullptr;
        };
    }

    /**
     * @brief 异步 future 调用
     */
    template<typename R,typename... Params>
    std::future<Result<R>> async_call(const std::string& name, Params&& ... ps) {
        std::function<Result<R>()> task = [name, ps..., this] () -> Result<R> {
            return call<R>(name, ps...);
        };
        auto promise = std::make_shared<std::promise<Result<R>>>();
        RpcClient::ptr self = shared_from_this();
        go [task, promise, self]() mutable {
            promise->set_value(task());
            self = nullptr;
        };
        return promise->get_future();
    }
    /**
     * @brief 订阅消息
     * @param[in] key 订阅的key
     * @param[in] func 回调函数
     */
    template<typename Func>
    void subscribe(const std::string& key, Func func) {
        {
            MutexType::Lock lock(m_sub_mtx);
            auto it = m_subHandle.find(key);
            if (it != m_subHandle.end()) {
                ACID_ASSERT2(false, "duplicated subscribe");
                return;
            }

           m_subHandle.emplace(key, std::move(func));
        }
        Serializer s;
        s << key;
        s.reset();
        Protocol::ptr response = Protocol::Create(Protocol::MsgType::RPC_SUBSCRIBE_REQUEST, s.toString(), 0);
        m_chan << response;
    }

    Socket::ptr getSocket() {
        return m_session->getSocket();
    }
private:
    /**
     * @brief rpc 连接对象的发送协程，通过 Channel 收集调用请求，并转发请求给服务器。
     */
    void handleSend();
    /**
     * @brief rpc 连接对象的接收协程，负责接收服务器发送的 response 响应并根据响应类型进行处理
     */
    void handleRecv();
    /**
     * @brief 通过序列号获取对应调用者的 Channel，将 response 放入 Channel 唤醒调用者
     */
    void handleMethodResponse(Protocol::ptr response);
    /**
     * @brief 处理发布消息
     */
    void handlePublish(Protocol::ptr proto);
    /**
     * @brief 实际调用
     * @param[in] s 序列化完的请求
     * @return 返回调用结果
     */
    template<typename R>
    Result<R> call(Serializer s) {
        Result<R> val;
        if (!m_session || !m_session->isConnected()) {
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");
            return val;
        }

        // 开启一个 Channel 接收调用结果
        Channel<Protocol::ptr> recvChan(1);
        // 本次调用的序列号
        uint32_t id = 0;
        std::map<uint32_t, Channel<Protocol::ptr>>::iterator it;
        {
            MutexType::Lock lock(m_mutex);
            id = m_sequenceId;
            // 将请求序列号与接收 Channel 关联
            it = m_responseHandle.emplace(m_sequenceId, recvChan).first;
            ++m_sequenceId;
        }

        // 创建请求协议，附带上请求 id
        Protocol::ptr request =
                Protocol::Create(Protocol::MsgType::RPC_METHOD_REQUEST,s.toString(), id);

        // 向 send 协程的 Channel 发送消息
        m_chan << request;

        acid::Timer::ptr timer;
        bool timeout = false;
        if( m_timeout != (uint64_t)-1 ){
            // 如果调用超时则关闭接收 Channel
            timer = IOManager::GetThis()->addTimer(m_timeout,[recvChan, &timeout]() mutable {
                timeout = true;
                recvChan.close();
            });
        }

        Protocol::ptr response;
        // 等待 response，Channel内部会挂起协程，如果有消息到达或者被关闭则会被唤醒
        recvChan >> response;

        if(timer){
            timer->cancel();
        }

        {
            MutexType::Lock lock(m_mutex);
            if (!m_isClose) {
                // 删除序列号与 Channel 的映射
                m_responseHandle.erase(it);
            }
        }

        if (timeout) {
            // 超时
            val.setCode(RPC_TIMEOUT);
            val.setMsg("call timeout");
            return val;
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
    bool m_isClose = true;
    bool m_isHeartClose = true;
    // 超时时间
    uint64_t m_timeout = -1;
    // 服务器的连接
    RpcSession::ptr m_session;
    // 序列号
    uint32_t m_sequenceId = 0;
    // 序列号到对应调用者协程的 Channel 映射
    std::map<uint32_t, Channel<Protocol::ptr>> m_responseHandle;
    // m_responseHandle 的 mutex
    MutexType m_mutex;
    // 消息发送通道
    Channel<Protocol::ptr> m_chan;
    // service provider心跳定时器
    Timer::ptr m_heartTimer;
    // 处理订阅的消息回调函数
    std::map<std::string, std::function<void(Serializer)>> m_subHandle;
    // 保护m_subHandle
    MutexType m_sub_mtx;
};


}
#endif //ACID_RPC_CLIENT_H
