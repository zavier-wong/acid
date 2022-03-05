//
// Created by zavier on 2022/1/13.
//

#ifndef ACID_RPC_SERVER_H
#define ACID_RPC_SERVER_H
#include <functional>
#include <memory>
#include "acid/net/socket_stream.h"
#include "acid/net/tcp_server.h"
#include "acid/log.h"
#include "acid/sync.h"
#include "acid/traits.h"
#include "protocol.h"
#include "rpc.h"
#include "rpc_session.h"
namespace acid::rpc {

/**
 * @brief 调用帮助类，展开 tuple 转发给函数
 * @param[in] func 调用的函数
 * @param[in] args 打包为 tuple 的函数参数
 * @param[in] index 编译期辅助展开 tuple
 * @return 返回函数调用结果
 */
template<typename Function, typename Tuple, std::size_t... Index>
auto invoke_impl(Function&& func, Tuple&& args, std::index_sequence<Index...>)
{
    return func(std::get<Index>(std::forward<Tuple>(args))...);
}
/**
 * @brief 调用帮助类
 * @param[in] func 调用的函数
 * @param[in] args 打包为 tuple 的函数参数
 * @return 返回函数调用结果
 */
template<typename Function, typename Tuple>
auto invoke(Function&& func, Tuple&& args)
{
    constexpr auto size = std::tuple_size<typename std::decay<Tuple>::type>::value;
    return invoke_impl(std::forward<Function>(func), std::forward<Tuple>(args), std::make_index_sequence<size>{});
}
/**
 * @brief 调用帮助类，判断返回类型，如果是 void 转换为 int8_t
 * @param[in] f 调用的函数
 * @param[in] args 打包为 tuple 的函数参数
 * @return 返回函数调用结果
 */
template<typename R, typename F, typename ArgsTuple>
auto call_helper(F f, ArgsTuple args) {
    if constexpr(std::is_same_v<R, void>) {
        invoke(f, args);
        return 0;
    } else {
        return invoke(f, args);
    }
}

/**
 * @brief RPC服务端
 */
class RpcServer : public TcpServer {
public:
    using ptr = std::shared_ptr<RpcServer>;
    RpcServer(IOManager* worker = IOManager::GetThis(),
               IOManager* accept_worker = IOManager::GetThis());
    bool bind(Address::ptr address) override;
    bool bindRegistry(Address::ptr address);
    bool start() override;
    /**
     * @brief 注册函数
     * @param[in] name 注册的函数名
     * @param[in] func 注册的函数
     */
    template<typename Func>
    void registerMethod(const std::string& name, Func func) {
        m_handlers[name] = [func, this](Serializer::ptr serializer, const std::string& arg) {
            proxy(func, serializer, arg);
        };
    }
    /**
     * @brief 设置RPC服务器名称
     * @param[in] name 名字
     */
    void setName(const std::string& name) override;
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
    Serializer::ptr call(const std::string& name, const std::string& arg);

    /**
     * @brief 调用代理
     * @param[in] fun 函数
     * @param[in] serializer 返回调用结果
     * @param[in] arg 函数参数字节流
     */
    template<typename F>
    void proxy(F fun, Serializer::ptr serializer, const std::string& arg) {
        typename function_traits<F>::stl_function_type func(fun);
        using Return = typename function_traits<F>::return_type;
        using Args = typename function_traits<F>::tuple_type;
        constexpr size_t N = function_traits<F>::arity;

        acid::rpc::Serializer s(arg);
        // 反序列化字节流，存为参数tuple
        Args args = s.getTuple<Args>(std::make_index_sequence<N>{});

        //通过包装函数调用
        return_type_t<Return> r = call_helper<Return>(func, args);

        Result<Return> val;
        val.setCode(acid::rpc::RPC_SUCCESS);
        val.setVal(r);
        (*serializer) << val;
    }
    /**
     * @brief 更新心跳定时器
     * @param[in] heartTimer 心跳定时器
     * @param[in] client 连接
     */
    void update(Timer::ptr& heartTimer, Socket::ptr client);
    /**
     * @brief 处理客户端连接
     * @param[in] client 客户端套接字
     */
    void handleClient(Socket::ptr client) override;
    /**
     * @brief 处理客户端请求
     * @param[in] client 客户端套接字
     */
    void handleRequest(Protocol::ptr proto, Channel<Protocol::ptr> chan);
    /**
     * @brief 处理客户端过程调用请求
     */
    Protocol::ptr handleMethodCall(Protocol::ptr proto);
    /**
     * 处理心跳包
     */
    Protocol::ptr handleHeartbeatPacket(Protocol::ptr proto);
private:
    /// 保存注册的函数
    std::map<std::string, std::function<void(Serializer::ptr, const std::string&)>> m_handlers;
    /// 服务中心连接
    RpcSession::ptr m_registry;
    /// 服务中心心跳定时器
    Timer::ptr m_heartTimer;
    /// 开放服务端口
    uint32_t m_port;
    // 和客户端的心跳时间 默认 40s
    uint64_t m_AliveTime = 40'000;
};

}
#endif //ACID_RPC_SERVER_H
