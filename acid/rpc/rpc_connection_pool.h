//
// Created by zavier on 2022/1/16.
//

#ifndef ACID_RPC_CONNECTION_POOL_H
#define ACID_RPC_CONNECTION_POOL_H
#include "acid/common/traits.h"
#include "route_strategy.h"
#include "rpc_client.h"
#include "serializer.h"
#include "rpc_session.h"

namespace acid::rpc {
/**
 * @brief RPC客户端连接池
 */
class RpcConnectionPool : public std::enable_shared_from_this<RpcConnectionPool> {
public:
    using ptr = std::shared_ptr<RpcConnectionPool>;
    using MutexType = co::co_mutex;

    RpcConnectionPool(uint64_t timeout_ms = -1, co::Scheduler* worker = &co_sched);
    ~RpcConnectionPool();
    /**
     * @brief 连接 RPC 服务中心
     * @param[in] address 服务中心地址
     */
    bool connect(Address::ptr address);

    /**
     * @brief 异步远程过程调用回调模式
     * @param[in] callback 回调函数
     * @param[in] name 函数名
     * @param[in] ps 可变参
     */
    template<typename... Params>
    void callback(const std::string& name, Params&&... ps) {
        static_assert(sizeof...(ps), "without a callback function");
        auto tp = std::make_tuple(ps...);
        constexpr auto size = std::tuple_size<typename std::decay<decltype(tp)>::type>::value;
        auto cb = std::get<size-1>(tp);
        static_assert(function_traits<decltype(cb)>{}.arity == 1, "callback type not support");
        using res = typename function_traits<decltype(cb)>::template args<0>::type;
        using rt = typename res::row_type;
        static_assert(std::is_invocable_v<decltype(cb), Result<rt>>, "callback type not support");
        RpcConnectionPool::ptr self = shared_from_this();
        go co_scheduler(m_worker) [cb = std::move(cb), name, tp = std::move(tp), self, this] {
            auto proxy = [&cb, &name, &tp, this]<std::size_t... Index>(std::index_sequence<Index...>){
                cb(call<rt>(name, std::get<Index>(tp)...));
            };
            proxy(std::make_index_sequence<size - 1>{});
            (void)self;
        };
    }
    /**
     * @brief 异步远程过程调用
     * @param[in] name 函数名
     * @param[in] ps 可变参
     * @return 返回 Channel
     */
    template<typename R,typename... Params>
    co::co_chan<Result<R>> async_call(const std::string& name, Params&& ... ps) {
        co::co_chan<Result<R>> chan(1);
        RpcConnectionPool::ptr self = shared_from_this();
        go co_scheduler(m_worker) [chan, name, ps..., self, this] () mutable {
            chan << call<R>(name, ps...);
            (void)self;
        };
        return chan;
    }
    /**
     * @brief 远程过程调用
     * @param[in] name 函数名
     * @param[in] ps 可变参
     * @return 返回调用结果
     */
    template<typename R, typename... Params>
    Result<R> call(const std::string& name, Params... ps) {
        std::unique_lock<co::co_mutex> lock(m_connMutex);
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
            std::erase(addrs, conn->second->getSocket()->getRemoteAddress()->toString());

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
        RouteStrategy<std::string>::ptr strategy =
                RouteEngine<std::string>::queryStrategy(Strategy::Random);

        if (addrs.size()) {
            const std::string ip = strategy->select(addrs);
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
    /**
     * @brief 订阅消息
     * @param[in] key 订阅的key
     * @param[in] func 回调函数
     */
    template<typename Func>
    void subscribe(const std::string& key, Func func) {
        {
            std::unique_lock<co::co_mutex> lock(m_sub_mtx);
            auto it = m_subHandle.find(key);
            if (it != m_subHandle.end()) {
                SPDLOG_LOGGER_ERROR(GetLogInstance(), "duplicated subscribe");
                return;
            }

            m_subHandle.emplace(key, std::move(func));
        }
        Serializer s;
        s << key;
        s.reset();
        Protocol::ptr response = Protocol::Create(Protocol::MsgType::RPC_SUBSCRIBE_REQUEST, s.toString(), 0);
        m_chan << response;
    }

    void close();
private:

    /**
     * @brief 服务发现
     * @param name 服务名称
     * @return 服务地址列表
     */
    std::vector<std::string> discover(const std::string& name);
    /**
     * @brief rpc 连接对象的发送协程，通过 Channel 收集调用请求，并转发请求给注册中心。
     */
    void handleSend();
    /**
     * @brief rpc 连接对象的接收协程，负责接收注册中心发送的 response 响应并根据响应类型进行处理
     */
    void handleRecv();
    /**
     * @brief 处理注册中心服务发现响应
     */
    void handleServiceDiscover(Protocol::ptr response);
    /**
     * @brief 处理发布消息
     */
    void handlePublish(Protocol::ptr proto);
private:
    std::atomic_bool m_isClose = true;
    std::atomic_bool m_isHeartClose = true;
    uint64_t m_timeout;
    // 保护 m_conns
    MutexType m_connMutex;
    // 服务名到全部缓存的服务地址列表映射
    std::map<std::string, std::vector<std::string>> m_serviceCache;
    // 服务名和服务地址的连接池
    std::map<std::string, RpcClient::ptr> m_conns;
    // 服务中心连接
    RpcSession::ptr m_registry;
    // 服务中心心跳定时器
    CycleTimerTocken m_heartTimer;
    // 注册中心消息发送通道
    co::co_chan<Protocol::ptr> m_chan;
    // 服务名到对应调用者协程的 Channel 映射
    std::map<std::string, co::co_chan<Protocol::ptr>> m_discover_handle;
    // m_discover_handle 的 mutex
    MutexType m_discover_mutex;
    // 处理订阅的消息回调函数
    std::map<std::string, std::function<void(Serializer)>> m_subHandle;
    // 保护m_subHandle
    MutexType m_sub_mtx;

    co::Scheduler* m_worker;
};

}
#endif //ACID_RPC_CONNECTION_POOL_H
