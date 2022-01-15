//
// Created by zavier on 2022/1/15.
//

#ifndef ACID_RPC_SERVICE_REGISTRY_H
#define ACID_RPC_SERVICE_REGISTRY_H
#include <string>
namespace acid::rpc {
/**
 * @brief RPC服务注册中心
 */
class RpcServiceRegistry {
public:
    /**
     * 为服务端提供注册
     * 将服务地址注册到对应服务名下
     * 断开连接后地址自动清除
     * @param serviceName 服务名称
     * @param serviceAddress 服务地址
     */
    void registerService(const std::string& serviceName, const std::string& serviceAddress) {

    }
private:

};
}
#endif //ACID_RPC_SERVICE_REGISTRY_H
