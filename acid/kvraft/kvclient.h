//
// Created by zavier on 2022/12/4.
//

#ifndef ACID_KVCLIENT_H
#define ACID_KVCLIENT_H

#include <optional>
#include "commom.h"
#include "../rpc/rpc_client.h"

namespace acid::kvraft {
using namespace acid::rpc;
class KVClient : public RpcClient {
public:
    using ptr = std::shared_ptr<KVClient>;
    using MutexType = co::co_mutex;
    KVClient(std::map<int64_t, std::string>& servers);
    ~KVClient();
    kvraft::Error Get(const std::string& key, std::string& value);
    kvraft::Error Put(const std::string& key, const std::string& value);
    kvraft::Error Append(const std::string& key, const std::string& value);
    kvraft::Error Delete(const std::string& key);
    kvraft::Error Clear();

    /**
     * 订阅频道，会阻塞 \n
     * @tparam Args std::string ...
     * @tparam Listener 用于监听订阅事件
     * @param channels 订阅的频道列表
     * @return 当成功取消所有的订阅后返回，否则一直阻塞
     */
    template<typename ... Args>
    void subscribe(PubsubListener::ptr listener, const Args& ... channels) {
        static_assert(sizeof...(channels));
        subscribe(std::move(listener), {channels...});
    }

    void subscribe(PubsubListener::ptr listener, const std::vector<std::string>& channels);

    /**
     * 取消订阅频道
     * @param channel 频道
     */
    void unsubscribe(const std::string& channel);

    /**
     * 模式订阅，阻塞 \n
     * 匹配规则：\n
     *  '?': 匹配 0 或 1 个字符 \n
     *  '*': 匹配 0 或更多个字符 \n
     *  '+': 匹配 1 或更多个字符 \n
     *  '@': 如果任何模式恰好出现一次 \n
     *  '!': 如果任何模式都不匹配 \n
     * @tparam Args std::string ...
     * @param listener 用于监听订阅事件
     * @param patterns 订阅的模式列表
     * @return 当成功取消所有的订阅后返回，否则一直阻塞
     */
    template<typename ... Args>
    void patternSubscribe(PubsubListener::ptr listener, const Args& ... patterns) {
        static_assert(sizeof...(patterns));
        patternSubscribe(std::move(listener), {patterns...});
    }

    void patternSubscribe(PubsubListener::ptr listener, const std::vector<std::string>& patterns);

    /**
     * 取消模式订阅频道
     * @param pattern 模式
     */
    void patternUnsubscribe(const std::string& pattern);
private:
    CommandResponse Command(CommandRequest& request);
    bool connect();
    int64_t nextLeaderId();
    static int64_t GetRandom();
private:
    std::unordered_map<int64_t, Address::ptr> m_servers;
    int64_t m_clientId;
    int64_t m_leaderId;
    int64_t m_commandId;
    std::atomic_bool m_stop;
    co::co_chan<bool> m_stopChan;
    std::vector<std::string> m_subs;
    MutexType m_pubsub_mutex;
    CycleTimerTocken m_heart;
};
}


#endif //ACID_KVCLIENT_H
