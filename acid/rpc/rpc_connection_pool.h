//
// Created by zavier on 2022/1/16.
//

#ifndef ACID_RPC_CONNECTION_POOL_H
#define ACID_RPC_CONNECTION_POOL_H
#include "acid/traits.h"
#include "acid/mutex.h"
#include "route_strategy.h"
#include "rpc_client.h"
#include "rpc_session.h"

namespace acid::rpc {
/**
 * @brief RPC客户端连接池
 */
class RpcConnectionPool {
public:
    using ptr = std::shared_ptr<RpcConnectionPool>;
    using MutexType = Mutex;

    RpcConnectionPool(uint64_t timeout_ms = -1);
    ~RpcConnectionPool();
    /**
     * @brief 连接 RPC 服务中心
     * @param[in] address 服务中心地址
     */
    bool connect(Address::ptr address);

    void stop();
    /**
     * @brief 用户可手动服务发现
     * @param name 服务名称
     * @return 通过服务的地址数量
     */
    [[maybe_unused]]
    size_t discover(const std::string& name) {
        std::vector<std::string> addrs = discover_impl(name);
        if (addrs.size()) {
            // 选择客户端负载均衡策略，根据路由策略选择服务地址
            acid::rpc::RouteStrategy<std::string>::ptr strategy =
                    acid::rpc::RouteEngine<std::string>::queryStrategy(
                            acid::rpc::RouteStrategy<std::string>::Random);
            const std::string& ip = strategy->select(addrs);
            Address::ptr address = Address::LookupAny(ip);
            // 选择的服务地址有效
            if (address) {
                RpcClient::ptr client = std::make_shared<RpcClient>();
                // 成功连接上服务器
                if (client->connect(address)) {
                    m_conns.template emplace(name, client);
                }
            }
        }
        return addrs.size();
    }

    /**
     * @brief 异步远程过程调用回调模式
     * @param[in] callback 回调函数
     * @param[in] name 函数名
     * @param[in] ps 可变参
     */
    template<typename R, typename... Params>
    void async_call(std::function<void(Result<R>)> callback, const std::string& name, Params&& ... ps) {

        std::function<Result<R>()> task = [name, ps..., this] () -> Result<R> {
            return call<R>(name, ps...);
        };

        acid::IOManager::GetThis()->submit([callback, task]{
            callback(task());
        });
    }
    /**
     * @brief 异步远程过程调用
     * @param[in] name 函数名
     * @param[in] ps 可变参
     * @return 返回调用结果 future
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
     * @brief 远程过程调用
     * @param[in] name 函数名
     * @param[in] ps 可变参
     * @return 返回调用结果
     */
    template<typename R, typename... Params>
    Result<R> call(const std::string& name, Params... ps) {
        MutexType::Lock lock(m_connMutex);
        // 从连接池里取出服务连接
        auto conn = m_conns.find(name);
        Result<R> result;
        if (conn != m_conns.end()) {
            lock.unlock();
            result = conn->second->template call<R>(name, ps...);
            if (result.getCode() != RPC_CLOSED) {
                return result;
            }
            lock.lock();
            // 移除失效连接
            std::vector<std::string>& addrs = m_serviceCache[name];
            addrs.erase(std::remove(addrs.begin(), addrs.end(),
                                    conn->second->getSocket()->getRemoteAddress()->toString()));

            m_conns.erase(name);
        }

        std::vector<std::string>& addrs = m_serviceCache[name];
        // 如果服务地址缓存为空则重新向服务中心请求服务发现
        if (addrs.empty()) {
            if (!m_registry || !m_registry->isConnected()) {
                Result<R> res;
                res.setCode(RPC_CLOSED);
                res.setMsg("registry closed");
                return res;
            }
            addrs = discover_impl(name);
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

        if (addrs.size()) {
            const std::string& ip = strategy->select(addrs);
            Address::ptr address = Address::LookupAny(ip);
            // 选择的服务地址有效
            if (address) {
                RpcClient::ptr client = std::make_shared<RpcClient>();
                // 成功连接上服务器
                if (client->connect(address)) {
                    m_conns.template emplace(name, client);
                    lock.unlock();
                    return client->template call<R>(name, ps...);
                }
            }
        }
        result.setCode(RPC_FAIL);
        result.setMsg("call fail");
        return result;
    }

private:

    /**
     * @brief 服务发现
     * @param name 服务名称
     * @return 服务地址列表
     */
    std::vector<std::string> discover_impl(const std::string& name);

private:
    uint64_t m_timeout;
    MutexType m_sendMutex;

    MutexType m_connMutex;
    /// 服务名到全部缓存的服务地址列表映射
    std::map<std::string, std::vector<std::string>> m_serviceCache;
    /// 服务名和服务地址的连接池
    std::map<std::string, RpcClient::ptr> m_conns;
    /// 服务中心连接
    RpcSession::ptr m_registry;

    /// 服务中心心跳定时器
    Timer::ptr m_heartTimer;
};

}
#endif //ACID_RPC_CONNECTION_POOL_H
