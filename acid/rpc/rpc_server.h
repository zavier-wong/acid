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
#include "acid/traits.h"
#include "rpc.h"
namespace acid::rpc {
/**
 * @brief RPC服务端
 */
class RpcServer : public TcpServer {
public:
    using ptr = std::shared_ptr<RpcServer>;
    RpcServer(IOManager* worker = IOManager::GetThis(),
               IOManager* accept_worker = IOManager::GetThis());

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
     * @brief 接受RPC客户端请求并序列化返回
     * @param[in] client 客户端
     * @param[in] func 注册的函数
     * @return 序列化的客户端请求
     */
    Serializer::ptr recvRequest(SocketStream::ptr client);
    /**
     * @brief 返回客户端请求结果
     * @param[in] client 客户端
     * @param[in] s 序列化结果
     */
    void sendResponse(SocketStream::ptr client, Serializer::ptr s);
    /**
     * @brief 调用服务端注册的函数，返回序列化完的结果
     * @param[in] name 函数名
     * @param[in] arg 函数参数字节流
     * @return 函数调用的序列化结果
     */
    Serializer::ptr call(const std::string& name, const std::string& arg);
    /**
     * @brief 处理客户端请求
     * @param[in] client 客户端套接字
     */
    void handleClient(Socket::ptr client) override;
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

private:
    /// 保存注册的函数
    std::map<std::string, std::function<void(Serializer::ptr, const std::string&)>> m_handlers;
};

}
#endif //ACID_RPC_SERVER_H
