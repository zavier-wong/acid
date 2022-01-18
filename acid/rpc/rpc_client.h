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
#include "protocol.h"
#include "route_strategy.h"
#include "rpc.h"
namespace acid::rpc {

/**
 * @brief 实际的序列化函数，利用折叠表达式展开参数包
 */
template<typename Tuple, std::size_t... Index>
void package_params_impl(Serializer& ds, const Tuple& t, std::index_sequence<Index...>)
{
    (ds << ... << std::get<Index>(t));
}

/**
 * @brief 将被打包为 tuple 的函数参数进行序列化
 */
template<typename... Args>
void package_params(Serializer& ds, const std::tuple<Args...>& t)
{
    package_params_impl(ds, t, std::index_sequence_for<Args...>{});
}

/**
 * @brief RPC客户端
 */
class RpcClient {
public:
    using ptr = std::shared_ptr<RpcClient>;

    RpcClient() {
        m_socket = Socket::CreateTCPSocket();
    }
    ~RpcClient() {
        m_socket->close();
    }
    /**
     * @brief 连接一个RPC服务器
     * @param[in] address 服务器地址
     */
    bool connect(Address::ptr address){
        return m_socket->connect(address, m_timeout);
    }

    /**
     * @brief 设置RPC调用超时时间
     * @param[in] address 服务器地址
     */
    void setTimeout(uint64_t timeout_ms) {
        m_timeout = timeout_ms;
        m_socket->setRecvTimeout(timeout_ms);
    }
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

    /**
     * @brief 发布/订阅模式预留接口，未实现
     */
    template<typename R>
    Result<R> subscribe(const std::string& name, std::function<void(const std::string&)> callback) {
        Serializer::ptr s = std::make_shared<Serializer>();
        (*s) << name;
        s->reset();
        return call<R>(s);
    }

    Socket::ptr getSocket() {
        return m_socket;
    }
private:
    /**
     * @brief 实际调用
     * @param[in] s 序列化完的请求
     * @return 返回调用结果
     */
    template<typename R>
    Result<R> call(Serializer::ptr s) {
        Result<R> val;
        if (!m_socket) {
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");
            return val;
        }
        Protocol::ptr request = std::make_shared<Protocol>();
        request->setMsgType(Protocol::MsgType::RPC_METHOD_REQUEST);
        request->setContent(s->toString());
        if (!sendRequest(request)) {
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");
            return val;
        }
        Protocol::ptr response = recvResponse();
        if (!response) {
            if (errno == ETIMEDOUT) {
                // 超时
                val.setCode(RPC_TIMEOUT);
                val.setMsg("call timeout");
            } else {
                val.setCode(RPC_CLOSED);
                val.setMsg("socket closed");
            }
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
    /**
     * @brief 发起网络请求
     * @param[in] p 请求协议
     * @return 返回调用是否成功
     */
    bool sendRequest(Protocol::ptr p) {
        SocketStream::ptr stream = std::make_shared<SocketStream>(m_socket, false);
        ByteArray::ptr bt = p->encode();
        if (stream->writeFixSize(bt, bt->getSize()) < 0) {
            return false;
        }
        return true;
    }
    /**
     * @brief 接受网络响应
     * @return 返回响应协议
     */
    Protocol::ptr recvResponse() {
        SocketStream::ptr stream = std::make_shared<SocketStream>(m_socket, false);
        Protocol::ptr response = std::make_shared<Protocol>();

        ByteArray::ptr byteArray = std::make_shared<ByteArray>();

        if (stream->readFixSize(byteArray, response->BASE_LENGTH) <= 0) {
            return nullptr;
        }

        byteArray->setPosition(0);
        response->decodeMeta(byteArray);

        if (!response->getContentLength()) {
            return response;
        }

        std::string buff;
        buff.resize(response->getContentLength());

        if (stream->readFixSize(&buff[0], buff.size()) <= 0) {
            return nullptr;
        }
        response->setContent(std::move(buff));
        return response;
    }

private:
    /// 超时时间
    uint64_t m_timeout = -1;
    /// 服务器的连接
    Socket::ptr m_socket;
};


}
#endif //ACID_RPC_CLIENT_H
