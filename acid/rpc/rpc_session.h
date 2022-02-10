//
// Created by zavier on 2022/2/10.
//

#ifndef ACID_RPC_SESSION_H
#define ACID_RPC_SESSION_H

#include "acid/net/socket_stream.h"
#include "protocol.h"

namespace acid::rpc {
/**
 * @brief rpc session 封装了协议的收发
 */
class RpcSession : public SocketStream {
public:
    using ptr = std::shared_ptr<RpcSession>;

    /**
     * @brief 构造函数
     * @param[in] sock Socket类型
     * @param[in] owner 是否托管Socket
     */
    RpcSession(Socket::ptr socket, bool owner = true);
    /**
     * @brief 接收协议
     * @return 如果返回空协议则接收失败
     */
    Protocol::ptr recvProtocol();
    /**
     * @brief 发送协议
     * @param[in] proto 要发送的协议
     * @return 发送大小
     */
    ssize_t sendProtocol(Protocol::ptr proto);
};
}
#endif //ACID_RPC_SESSION_H
