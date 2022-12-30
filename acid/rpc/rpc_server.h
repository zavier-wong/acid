//
// Created by zavier on 2022/1/13.
//

#ifndef ACID_RPC_SERVER_H
#define ACID_RPC_SERVER_H
#include <functional>
#include <memory>
#include "acid/net/socket_stream.h"
#include "acid/net/tcp_server.h"
#include "acid/common/traits.h"
#include "protocol.h"
#include "pubsub.h"
#include "rpc.h"
#include "rpc_session.h"
namespace acid::rpc {
/**
 * @brief RPC服务端
 */
class RpcServer : public TcpServer {
public:
    using ptr = std::shared_ptr<RpcServer>;
    using MutexType = co::co_mutex;

    RpcServer();
    ~RpcServer();
    bool bind(Address::ptr address) override;
    bool bindRegistry(Address::ptr address);
    void start() override;
    void stop() override;
    /**
     * @brief 注册函数
     * @param[in] name 注册的函数名
     * @param[in] func 注册的函数
     */
    template<typename Func>
    void registerMethod(const std::string& name, Func func) {
        m_handlers[name] = [func, this](Serializer serializer, const std::string& arg) {
            proxy(func, serializer, arg);
        };
    }
    /**
     * @brief 设置RPC服务器名称
     * @param[in] name 名字
     */
    void setName(const std::string& name) override;

    /**
     * @brief 对一个频道发布消息
     * @param channel 频道名
     * @param message 消息
     */
    void publish(const std::string& channel, const std::string& message);

protected:
    /**
     * @brief 向服务注册中心发起注册
     * @param[in] name 注册的函数名
     */
    void registerService(const std::string& name);
    /**
     * @brief 调用服务端注册的函数，返回序列化完的结果
     * @param[in] name 函数名
     * @param[in] arg 函数参数字节流
     * @return 函数调用的序列化结果
     */
    Serializer call(const std::string& name, const std::string& arg);

    /**
     * @brief 调用代理
     * @param[in] fun 函数
     * @param[in] serializer 返回调用结果
     * @param[in] arg 函数参数字节流
     */
    template<typename F>
    void proxy(F fun, Serializer serializer, const std::string& arg) {
        typename function_traits<F>::stl_function_type func(fun);
        using Return = typename function_traits<F>::return_type;
        using Args = typename function_traits<F>::tuple_type;

        acid::rpc::Serializer s(arg);
        // 反序列化字节流，存为参数tuple
        Args args;
        try {
            s >> args;
        } catch (...) {
            Result<Return> val;
            val.setCode(acid::rpc::RPC_NO_MATCH);
            val.setMsg("params not match");
            serializer << val;
            return;
        }

        return_type_t<Return> rt{};

        constexpr auto size = std::tuple_size<typename std::decay<Args>::type>::value;
        auto invoke = [&func, &args]<std::size_t... Index>(std::index_sequence<Index...>){
            return func(std::get<Index>(std::forward<Args>(args))...);
        };

        if constexpr(std::is_same_v<Return, void>) {
            invoke(std::make_index_sequence<size>{});
        } else {
            rt = invoke(std::make_index_sequence<size>{});
        }

        Result<Return> val;
        val.setCode(acid::rpc::RPC_SUCCESS);
        val.setVal(rt);
        serializer << val;
    }
    /**
     * @brief 处理客户端连接
     * @param[in] client 客户端套接字
     */
    void handleClient(Socket::ptr client) override;
    /**
     * @brief 处理客户端过程调用请求
     */
    Protocol::ptr handleMethodCall(Protocol::ptr proto);
    /**
     * @brief 处理心跳包
     */
    Protocol::ptr handleHeartbeatPacket(Protocol::ptr proto);
    /**
     * @brief 处理发布订阅
     */
    Protocol::ptr handlePubsubRequest(Protocol::ptr proto, Socket::ptr client);

    void subscribe(const std::string& channel, Socket::ptr client);

    void unsubscribe(const std::string& channel, Socket::ptr client);

    void patternSubscribe(const std::string& pattern, Socket::ptr client);

    void patternUnsubscribe(const std::string& pattern, Socket::ptr client);
private:
    // 保存注册的函数
    std::map<std::string, std::function<void(Serializer, const std::string&)>> m_handlers;
    // 服务中心连接
    RpcSession::ptr m_registry;
    // 服务中心心跳定时器
    CycleTimerTocken m_heartTimer;
    // 开放服务端口
    uint32_t m_port;
    // 和客户端的心跳时间 默认 40s
    uint64_t m_aliveTime;
    //用于保存所有频道的订阅关系
    std::map<std::string, std::list<Socket::ptr> > m_pubsub_channels;
    //保存所有模式订阅关系
    std::list<std::pair<std::string, Socket::ptr>> m_pubsub_patterns;

    MutexType m_pubsub_mutex;
};

}
#endif //ACID_RPC_SERVER_H
