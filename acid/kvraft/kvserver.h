//
// Created by zavier on 2022/12/3.
//

#ifndef ACID_KVSERVER_H
#define ACID_KVSERVER_H

#include "../raft/raft_node.h"
#include "commom.h"

namespace acid::kvraft {
using namespace acid::raft;
class KVServer {
public:
    using ptr = std::shared_ptr<KVServer>;
    using MutexType = co::co_mutex;
    using KVMap = std::map<std::string, std::string>;

    KVServer(std::map<int64_t, std::string>& servers, int64_t id, Persister::ptr persister, int64_t maxRaftState = 1000);
    ~KVServer();
    void start();
    void stop();
    CommandResponse handleCommand(CommandRequest request);
    CommandResponse Get(const std::string& key);
    CommandResponse Put(const std::string& key, const std::string& value);
    CommandResponse Append(const std::string& key, const std::string& value);
    CommandResponse Delete(const std::string& key);
    CommandResponse Clear();
    [[nodiscard]]
    const KVMap& getData() const { return m_data;}
private:
    void applier();
    void saveSnapshot(int64_t index);
    void readSnapshot(Snapshot::ptr snap);
    bool isDuplicateRequest(int64_t client, int64_t command);
    bool needSnapshot();
    CommandResponse applyLogToStateMachine(const CommandRequest& request);
private:
    MutexType m_mutex;
    int64_t m_id;
    co::co_chan<raft::ApplyMsg> m_applychan;

    KVMap m_data;
    Persister::ptr m_persister;
    std::unique_ptr<RaftNode> m_raft;

    std::map<int64_t, std::pair<int64_t, CommandResponse>> m_lastOperation;
    std::map<int64_t, co::co_chan<CommandResponse>> m_nofiyChans;

    int64_t m_lastApplied = 0;
    int64_t m_maxRaftState = -1;
};
}
#endif //ACID_KVSERVER_H
