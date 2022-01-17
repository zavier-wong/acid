//
// Created by zavier on 2022/1/16.
//

#ifndef ACID_RPC_CONNECTION_POLL_H
#define ACID_RPC_CONNECTION_POLL_H
#include "acid/ds/lru_map.h"
#include "route_strategy.h"
#include "rpc_client.h"

namespace acid::rpc {

class RpcConnectionPool : std::enable_shared_from_this<RpcConnectionPool>{
public:
    using ptr = std::shared_ptr<RpcConnectionPool>;
    using RWMutexType = RWMutex;

    RpcConnectionPool(size_t capacity,uint64_t timeout_ms = -1)
        : m_timeout(timeout_ms)
        , m_conns(capacity){
        m_registry = Socket::CreateTCPSocket();
    }

    /**
     * @brief 连接 RPC 服务中心
     * @param[in] address 服务中心地址
     */
    bool connect(Address::ptr address){
        if (!m_registry->connect(address, m_timeout)) {
            return false;
        }
        //Protocol::ptr proto = std::make_shared<Protocol>();

        return true;
    }

    void run() {
        auto self = shared_from_this();
        acid::IOManager::GetThis()->submit([self]{

        });
    }
    /**
     * @brief 远程过程调用
     * @param[in] name 函数名
     * @param[in] ps 可变参
     * @return 返回调用结果
     */
    template<typename R>
    Result<R> call(const std::string& name) {
        // 从连接池里取出服务连接
        auto conn = m_conns.get(name);
        Result<R> result;
        if (conn) {
            result = (*conn)->template call<R>(name);
            if (result.getCode() != RPC_CLOSED) {
                return result;
            }
            // 移除失效连接
            std::vector<std::string> addrs = m_serviceCache[name];
            addrs.erase(std::remove(addrs.begin(), addrs.end(),
                                    (*conn)->getSocket()->getRemoteAddress()->toString()));

            m_conns.remove(name);
        }

        std::vector<std::string>& addrs = m_serviceCache[name];
        // 如果服务地址缓存为空则重新向服务中心请求服务发现
        if (addrs.empty()) {
            addrs = discover(name);
            // 如果没有发现服务返回错误
            if (addrs.empty()) {
                Result<R> res;
                res.setCode(RPC_NO_METHOD);
                res.setMsg("no method:" + name);
                return res;
            }
        }

        // 选择客户端负载均衡策略，根据路由策略选择服务地址
        acid::rpc::RouteStrategy<std::string>::ptr strategy =
                acid::rpc::RouteEngine<std::string>::queryStrategy(
                        acid::rpc::RouteStrategy<std::string>::Random);

        while (addrs.size()) {
            const std::string& ip = strategy->select(addrs);
            Address::ptr address = Address::LookupAny(ip);
            // 选择的服务地址有效
            if (address) {
                RpcClient::ptr client = std::make_shared<RpcClient>();
                // 成功连接上服务器
                if (client->connect(address)) {
                    m_conns.set(name, client);
                    return call<R>(name);
                }
            }
        }
        result.setCode(RPC_FAIL);
        result.setMsg("call fail");
        return result;
    }
    /**
     * @brief 远程过程调用
     * @param[in] name 函数名
     * @param[in] ps 可变参
     * @return 返回调用结果
     */
    template<typename R, typename... Params>
    Result<R> call(const std::string& name, Params... ps) {
        // 从连接池里取出服务连接
        auto conn = m_conns.get(name);
        Result<R> result;
        if (conn) {
            result = (*conn)->template call<R>(name, ps...);
            if (result.getCode() != RPC_CLOSED) {
                return result;
            }
            // 移除失效连接
            std::vector<std::string> addrs = m_serviceCache[name];
            addrs.erase(std::remove(addrs.begin(), addrs.end(),
                                    (*conn)->getSocket()->getRemoteAddress()->toString()));

            m_conns.remove(name);
        }

        std::vector<std::string>& addrs = m_serviceCache[name];
        // 如果服务地址缓存为空则重新向服务中心请求服务发现
        if (addrs.empty()) {
            addrs = discover(name);
            // 如果没有发现服务返回错误
            if (addrs.empty()) {
                Result<R> res;
                res.setCode(RPC_NO_METHOD);
                res.setMsg("no method:" + name);
                return res;
            }
        }

        // 选择客户端负载均衡策略，根据路由策略选择服务地址
        acid::rpc::RouteStrategy<std::string>::ptr strategy =
                acid::rpc::RouteEngine<std::string>::queryStrategy(
                        acid::rpc::RouteStrategy<std::string>::Random);

        while (addrs.size()) {
            const std::string& ip = strategy->select(addrs);
            Address::ptr address = Address::LookupAny(ip);
            // 选择的服务地址有效
            if (address) {
                RpcClient::ptr client = std::make_shared<RpcClient>();
                // 成功连接上服务器
                if (client->connect(address)) {
                    m_conns.set(name, client);
                    return call<R>(name, ps...);
                }
            }
        }
        result.setCode(RPC_FAIL);
        result.setMsg("call fail");
        return result;
    }

//private:

    /**
     * @brief 发现服务的地址
     * @param name 服务名称
     * @return 服务地址列表
     */
    std::vector<std::string> discover(const std::string& name);

    /**
     * @brief 接收服务中心响应
     * @return 客户端请求协议
     */
    Protocol::ptr recvResponse();

    /**
     * @brief 向服务中心发起请求
     * @param[in] p 发送协议
     */
    bool sendRequest(Protocol::ptr p);

private:
    uint32_t m_maxSize;
    uint64_t m_timeout;
    RWMutexType m_mutex;
    //std::list<RpcClient::ptr> m_conns;
    // 服务名到全部缓存的服务地址列表映射
    std::map<std::string, std::vector<std::string>> m_serviceCache;
    // 服务名和服务地址的连接池
    LRUMap<std::string, RpcClient::ptr> m_conns;
    Socket::ptr m_registry;
};

}
#endif //ACID_RPC_CONNECTION_POLL_H
