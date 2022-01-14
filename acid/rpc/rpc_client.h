//
// Created by zavier on 2022/1/13.
//

#ifndef ACID_RPC_CLIENT_H
#define ACID_RPC_CLIENT_H
#include <memory>
#include <functional>
#include "acid/net/socket.h"
#include "acid/net/socket_stream.h"
#include "rpc.h"
namespace acid::rpc {
/**
 * @brief RPC客户端
 */
class RpcClient {
public:
    using ptr = std::shared_ptr<RpcClient>;

    RpcClient() {
        m_socket = Socket::CreateTCPSocket();
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

        Serializer ds;
        ds << name;
        package_params(ds, args);
        ds.reset();
        return call<R>(ds);
    }
    /**
     * @brief 无参数的调用
     * @param[in] name 函数名
     * @return 返回调用结果
     */
    template<typename R>
    Result<R> call(const std::string& name) {
        Serializer ds;
        ds << name;
        ds.reset();
        return call<R>(ds);
    }

private:
    /**
     * @brief 实际调用
     * @param[in] s 序列化完的请求
     * @return 返回调用结果
     */
    template<typename R>
    Result<R> call(Serializer& s) {
        Result<R> val;
        if (!m_socket) {
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");

            return val;
        }

        if (!sendRequest(s)) {
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");
            return val;
        }
        val = recvResponse<R>();
        return val;
    }
    /**
     * @brief 发起网络请求
     * @param[in] s 序列化的函数请求
     * @return 返回调用是否成功
     */
    bool sendRequest(Serializer& s) {
        SocketStream::ptr stream = std::make_shared<SocketStream>(m_socket, false);
        const std::string& str = s.toString();

        ByteArray::ptr byteArray = std::make_shared<ByteArray>();
        byteArray->writeFint32(s.size());
        byteArray->setPosition(0);
        if (stream->writeFixSize(byteArray, byteArray->getSize()) < 0) {
            return false;
        }
        if (stream->writeFixSize(s.getByteArray(), s.size()) < 0) {
            return false;
        }

        return true;
    }
    /**
     * @brief 接受网络响应
     * @return 返回调用结果
     */
    template<typename R>
    Result<R> recvResponse() {
        SocketStream::ptr stream = std::make_shared<SocketStream>(m_socket, false);
        Result<R> val;
        constexpr int LengthSize = 4;

        ByteArray::ptr byteArray = std::make_shared<ByteArray>();

        if (stream->readFixSize(byteArray, LengthSize) <= 0) {
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");
            return val;
        }
        byteArray->setPosition(0);
        int length = byteArray->readFint32();
        byteArray->clear();

        if (stream->readFixSize(byteArray, length) < 0) {
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

        byteArray->setPosition(0);

        Serializer::ptr serializer = std::make_shared<Serializer>(byteArray);

        (*serializer) >> val;
        return val;
    }

private:
    /// 超时时间
    uint64_t m_timeout = -1;
    /// 服务器的连接
    Socket::ptr m_socket;
};

}
#endif //ACID_RPC_CLIENT_H
