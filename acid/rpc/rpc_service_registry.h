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
#include "acid/mutex.h"
#include "acid/net/tcp_server.h"
#include "acid/net/socket_stream.h"
#include "protocol.h"
namespace acid::rpc {
/**
 * @brief RPC服务注册中心
 * @details 接收客户端服务发现请求。接收服务端服务注册请求，断开连接后移除服务。
 * 以上统称为服务注册中心的用户
 */
class RpcServiceRegistry : public TcpServer {
public:
    using ptr = std::shared_ptr<RpcServiceRegistry>;
    using RWMutexType = RWMutex;
    RpcServiceRegistry(IOManager* worker = IOManager::GetThis(),
                        IOManager* accept_worker = IOManager::GetThis());

    /**
     * @brief 设置 RPC 服务注册中心名称
     * @param[in] name 名字
     */
    void setName(const std::string& name) override {
        TcpServer::setName(name);
    }

protected:

    /**
     * @brief 更新心跳定时器
     * @param[in] heartTimer 心跳定时器
     * @param[in] client 连接
     */
    void update(Timer::ptr& heartTimer, Socket::ptr client);

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
    RWMutexType m_mutex;
    // 允许心跳超时的时间 默认 40s
    uint64_t m_AliveTime = 40'000;
};


}
#endif //ACID_RPC_SERVICE_REGISTRY_H
