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

class KVClient {
public:
    using ptr = std::shared_ptr<KVClient>;
    KVClient(std::map<int64_t, std::string>& servers);
    ~KVClient();
    std::string Get(const std::string& key);
    void Put(const std::string& key, const std::string& value);
    void Append(const std::string& key, const std::string& value);
    bool Delete(const std::string& key);
    void Clear();
private:
    CommandResponse Command(CommandRequest& request);
    bool connect(int64_t id);
    int64_t nextLeaderId();
    static int64_t GetRandom();
private:
    std::unordered_map<int64_t, std::pair<Address::ptr, RpcClient::ptr>> m_servers;
    int64_t m_clientId;
    int64_t m_leaderId;
    int64_t m_commandId;
    std::atomic_bool m_stop;
    co::co_chan<bool> m_stopChan;
};
}


#endif //ACID_KVCLIENT_H
