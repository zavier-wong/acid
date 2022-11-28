//
// Created by zavier on 2022/1/15.
//

#ifndef ACID_RPC_SERVICE_REGISTRY_H
#define ACID_RPC_SERVICE_REGISTRY_H
#include <memory>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "acid/net/tcp_server.h"
#include "acid/net/socket_stream.h"
#include "protocol.h"
#include "rpc_session.h"
#include "serializer.h"
namespace acid::rpc {
/**
 * @brief RPC服务注册中心
 * @details 接收客户端服务发现请求。接收服务端服务注册请求，断开连接后移除服务。
 * 以上统称为服务注册中心的用户
 */
class RpcServiceRegistry : public TcpServer {
public:
    using ptr = std::shared_ptr<RpcServiceRegistry>;
    using MutexType = co::co_mutex;
    RpcServiceRegistry();

    ~RpcServiceRegistry();

    void start() override;
    void stop() override;

    /**
     * @brief 发布消息
     * @param[in] key 发布的key
     * @param[in] data 支持 Serializer 的都可以发布
     */
    template <typename T>
    void publish(const std::string& key, T data) {
        {
            std::unique_lock<co::co_mutex> lock(m_sub_mtx);
            if (m_subscribes.empty()) {
                return;
            }
        }
        Serializer s;
        s << key << data;
        s.reset();
        Protocol::ptr pub = Protocol::Create(Protocol::MsgType::RPC_PUBLISH_REQUEST, s.toString(), 0);
        std::unique_lock<co::co_mutex> lock(m_sub_mtx);
        auto range = m_subscribes.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            auto conn = it->second.lock();
            if (conn == nullptr || !conn->isConnected()) {
                continue;
            }
            conn->sendProtocol(pub);
        }
    }

protected:

    /**
     * @brief 处理端请求
     * @param[in] client 用户套接字
     */
    void handleClient(Socket::ptr client) override;
    /**
     * 为服务端提供服务注册
     * 将服务地址注册到对应服务名下
     * 断开连接后地址自动清除
     * @param serviceName 服务名称
     * @param serviceAddress 服务地址
     */
    Protocol::ptr handleRegisterService(Protocol::ptr p, Address::ptr address);
    /**
     * 移除注册服务
     * @param sock 移除的服务地址
     */
    void handleUnregisterService(Address::ptr address);
    /**
     * 为客户端提供服务发现
     * @param serviceName 服务名称
     * @return 服务地址列表
     */
    Protocol::ptr handleDiscoverService(Protocol::ptr p);
    /**
     * 处理 provider 初次连接时的事件，获取开放服务的端口
     * @param serviceName 服务名称
     * @return 服务地址列表
     */
    Address::ptr handleProvider(Protocol::ptr p, Socket::ptr sock);
    /**
     * 处理心跳包
     */
    Protocol::ptr handleHeartbeatPacket(Protocol::ptr p);
    /**
     * @brief 处理订阅请求
     */
    Protocol::ptr handleSubscribe(Protocol::ptr proto, RpcSession::ptr client);
private:
    /**
     * 维护服务名和服务地址列表的多重映射
     * serviceName -> serviceAddress1
     *             -> serviceAddress2
     *             ...
     */
    std::multimap<std::string, std::string> m_services;
    // 维护服务地址到迭代器的映射
    std::map<std::string, std::vector<std::multimap<std::string, std::string>::iterator>> m_iters;
    MutexType m_mutex;
    // 允许心跳超时的时间
    uint64_t m_aliveTime;
    // 订阅的客户端
    std::unordered_multimap<std::string, std::weak_ptr<RpcSession>> m_subscribes;
    // 保护 m_subscribes
    MutexType m_sub_mtx;
    // 停止清理订阅协程
    std::atomic_bool m_stop_clean = false;
    // 等待清理协程停止
    co::co_chan<bool> m_clean_chan;
};


}
#endif //ACID_RPC_SERVICE_REGISTRY_H
