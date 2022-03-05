//
// Created by zavier on 2022/1/15.
//

#ifndef ACID_ROUTE_STRATEGY_H
#define ACID_ROUTE_STRATEGY_H

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <memory>
#include <mutex>
#include <vector>

namespace acid::rpc {
enum class Strategy {
    //随机算法
    Random,
    //轮询算法
    Polling,
    //源地址hash算法
    HashIP
};
/**
 * @brief 路由策略接口
 * @details 通用类型负载均衡路由引擎（含随机、轮询、哈希）, 由客户端使用，在客户端实现负载均衡
 */
template<class T>
class RouteStrategy {
public:
    using ptr = std::shared_ptr<RouteStrategy>;
    virtual ~RouteStrategy() {}
    /**
    * @brief负载策略算法
    * @param list 原始列表
    * @return
    */
    virtual T& select(std::vector<T>& list) = 0;
};
namespace impl {
    /**
     * @brief 随机策略
     */
    template<class T>
    class RandomRouteStrategyImpl : public RouteStrategy<T> {
    public:
        T& select(std::vector<T>& list) {
            srand((unsigned)time(NULL));
            return list[rand() % list.size()];
        }
    };
    /**
     * @brief 轮询策略
     */
    template<class T>
    class PollingRouteStrategyImpl : public RouteStrategy<T> {
    public:
        T& select(std::vector<T>& list) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_index >= (int)list.size()) {
                m_index = 0;
            }
            return list[m_index++];
        }
    private:
        int m_index = 0;
        std::mutex m_mutex;
    };
    static std::string GetLocalHost();
    /**
     * @brief 基于IP的哈希策略
     */
    template<class T>
    class HashIPRouteStrategyImpl : public RouteStrategy<T> {
    public:
        T& select(std::vector<T>& list) {
            static std::string host = GetLocalHost();
            //保证程序健壮性,若未取到域名,则采用随机算法
            if(host.empty()){
                return RandomRouteStrategyImpl<T>{}.select(list);
            }
            //获取源地址对应的hashcode
            size_t hashCode = std::hash<std::string>()(host);
            //获取服务列表大小
            size_t size = list.size();

            return list[hashCode % size];
        }

    };

    std::string GetLocalHost() {
        int sockfd;
        struct ifconf ifconf;
        struct ifreq *ifreq = nullptr;
        char buf[512];//缓冲区
        //初始化ifconf
        ifconf.ifc_len =512;
        ifconf.ifc_buf = buf;
        if ((sockfd = socket(AF_INET,SOCK_DGRAM,0))<0)
        {
            return std::string{};
        }
        ioctl(sockfd, SIOCGIFCONF, &ifconf); //获取所有接口信息
        //接下来一个一个的获取IP地址
        ifreq = (struct ifreq*)ifconf.ifc_buf;
        for (int i=(ifconf.ifc_len/sizeof (struct ifreq)); i>0; i--)
        {
            if(ifreq->ifr_flags == AF_INET){ //for ipv4
                if (ifreq->ifr_name == std::string("eth0")) {
                    return std::string(inet_ntoa(((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr));
                }
                ifreq++;
            }
        }
        return std::string{};
    }
}

/**
 * @brief: 路由均衡引擎
 */
template<class T>
class RouteEngine {
public:
    static typename RouteStrategy<T>::ptr queryStrategy(Strategy routeStrategyEnum) {
        switch (routeStrategyEnum){
            case Strategy::Random:
                return s_randomRouteStrategy;
            case Strategy::Polling:
                return std::make_shared<impl::PollingRouteStrategyImpl<T>>();
            case Strategy::HashIP:
                return s_hashIPRouteStrategy ;
            default:
                return s_randomRouteStrategy ;
        }
    }
private:
    static typename RouteStrategy<T>::ptr s_randomRouteStrategy;
    static typename RouteStrategy<T>::ptr s_hashIPRouteStrategy;
};
template<class T>
typename RouteStrategy<T>::ptr RouteEngine<T>::s_randomRouteStrategy = std::make_shared<impl::RandomRouteStrategyImpl<T>>();
template<class T>
typename RouteStrategy<T>::ptr RouteEngine<T>::s_hashIPRouteStrategy = std::make_shared<impl::HashIPRouteStrategyImpl<T>>();
}
#endif //ACID_ROUTE_STRATEGY_H
