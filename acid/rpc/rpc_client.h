//
// Created by zavier on 2022/1/13.
//

#ifndef ACID_RPC_CLIENT_H
#define ACID_RPC_CLIENT_H
#include <memory>
#include <functional>
#include "acid/net/socket.h"
#include "acid/net/socket_stream.h"
#include "acid/common/traits.h"
#include "protocol.h"
#include "pubsub.h"
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
    using MutexType = co::co_mutex;

    /**
     * @brief 构造函数
     * @param[in] worker 调度器
     */
    RpcClient();

    ~RpcClient();

    void close();

    /**
     * @brief 设置心跳
     * @param[in] is_auto 是否开启心跳包
     */
    void setHeartbeat(bool is_auto) {
        m_auto_heartbeat = is_auto;
    }

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
    template<typename... Params>
    void callback(const std::string& name, Params&&... ps) {
        static_assert(sizeof...(ps), "without a callback function");
        auto tp = std::make_tuple(ps...);
        constexpr auto size = std::tuple_size<typename std::decay<decltype(tp)>::type>::value;
        auto cb = std::get<size-1>(tp);
        static_assert(function_traits<decltype(cb)>{}.arity == 1, "callback type not support");
        using res = typename function_traits<decltype(cb)>::template args<0>::type;
        using rt = typename res::row_type;
        static_assert(std::is_invocable_v<decltype(cb), Result<rt>>, "callback type not support");
        RpcClient::ptr self = shared_from_this();
        go [cb = std::move(cb), name, tp = std::move(tp), self, this] {
            auto proxy = [&cb, &name, &tp, this]<std::size_t... Index>(std::index_sequence<Index...>){
                cb(call<rt>(name, std::get<Index>(tp)...));
            };
            proxy(std::make_index_sequence<size - 1>{});
            (void)self;
        };
    }
    /**
     * @brief 异步调用，返回一个 Channel
     */
    template<typename R,typename... Params>
    co_chan<Result<R>> async_call(const std::string& name, Params&& ... ps) {
        co_chan<Result<R>> chan;
        RpcClient::ptr self = shared_from_this();
        go [self, chan, name, ps..., this] () mutable {
            chan << call<R>(name, ps...);
            self = nullptr;
        };
        return chan;
    }


    bool isSubscribe();

    /**
     * 取消订阅频道
     * @param channel 频道
     */
    void unsubscribe(const std::string& channel);

    /**
     * 订阅频道，会阻塞 \n
     * @tparam Args std::string ...
     * @tparam Listener 用于监听订阅事件
     * @param channels 订阅的频道列表
     * @return 当成功取消所有的订阅后返回 true，连接断开或其他导致订阅失败将返回 false
     */
    template<typename ... Args>
    bool subscribe(PubsubListener::ptr listener, const Args& ... channels) {
        static_assert(sizeof...(channels));
        return subscribe(std::move(listener), {channels...});
    }

    /**
    * 订阅频道，会阻塞 \n
    * @tparam Listener 用于监听订阅事件
    * @param channels 订阅的频道列表
    * @return 当成功取消所有的订阅后返回 true，连接断开或其他导致订阅失败将返回 false
    */
    bool subscribe(PubsubListener::ptr listener, const std::vector<std::string>& channels);

    /**
     * 取消模式订阅频道
     * @param pattern 模式
     */
    void patternUnsubscribe(const std::string& pattern);

    /**
     * 模式订阅，阻塞 \n
     * 匹配规则：\n
     *  '?': 匹配 0 或 1 个字符 \n
     *  '*': 匹配 0 或更多个字符 \n
     *  '+': 匹配 1 或更多个字符 \n
     *  '@': 如果任何模式恰好出现一次 \n
     *  '!': 如果任何模式都不匹配 \n
     * @tparam Args std::string ...
     * @param listener 用于监听订阅事件
     * @param patterns 订阅的模式列表
     * @return 当成功取消所有的订阅后返回 true，连接断开或其他导致订阅失败将返回 false
     */
    template<typename ... Args>
    bool patternSubscribe(PubsubListener::ptr listener, const Args& ... patterns) {
        static_assert(sizeof...(patterns));
        return patternSubscribe(std::move(listener), {patterns...});
    }

    bool patternSubscribe(PubsubListener::ptr listener, const std::vector<std::string>& patterns);

    /**
     * 向一个频道发布消息
     * @param channel 频道
     * @param message 消息
     * @return 是否发生成功
     */
    bool publish(const std::string& channel, const std::string& message);

    template<typename T>
    bool publish(const std::string& channel, const T& data) {
        Serializer s;
        s << data;
        s.reset();
        return publish(channel, s.toString());
    }

    Socket::ptr getSocket() { return m_session->getSocket(); }

    bool isClose() { return !m_session || !m_session->isConnected(); }

private:
    /**
     * @brief rpc 连接对象的发送协程，通过 Channel 收集调用请求，并转发请求给服务器。
     */
    void handleSend();
    /**
     * @brief rpc 连接对象的接收协程，负责接收服务器发送的 response 响应并根据响应类型进行处理
     */
    void handleRecv();

    void handlePublish(Protocol::ptr proto);
    /**
     * @brief 通过序列号获取对应调用者的 Channel，将 response 放入 Channel 唤醒调用者
     */
    void recvProtocol(Protocol::ptr response);

    std::pair<Protocol::ptr, bool> sendProtocol(Protocol::ptr request);

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

        Protocol::ptr request = Protocol::Create(Protocol::MsgType::RPC_METHOD_REQUEST, s.toString());

        auto [response, timeout] = sendProtocol(request);

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
        try {
            serializer >> val;
        } catch (...) {
            val.setCode(RPC_NO_MATCH);
            val.setMsg("return value not match");
        }
        return val;
    }

private:
    // 是否自动开启心跳包
    bool m_auto_heartbeat = true;
    std::atomic_bool m_isClose = true;
    std::atomic_bool m_isHeartClose = true;
    co::co_chan<bool> m_recvCloseChan;
    // 超时时间
    uint64_t m_timeout = -1;
    // 服务器的连接
    RpcSession::ptr m_session;
    // 序列号
    uint32_t m_sequenceId = 0;
    // 序列号到对应调用者协程的 Channel 映射
    std::map<uint32_t, co::co_chan<Protocol::ptr>> m_responseHandle;
    // m_responseHandle 的 mutex
    MutexType m_mutex;
    // 消息发送通道
    co::co_chan<Protocol::ptr> m_chan;
    // service provider心跳定时器
    CycleTimerTocken m_heartTimer;
    // 订阅监听器
    PubsubListener::ptr m_listener;
    // 订阅的频道
    std::map<std::string, co::co_chan<bool>> m_subs;

    MutexType m_pubsub_mutex;
};


}
#endif //ACID_RPC_CLIENT_H
