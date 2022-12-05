//
// Created by zavier on 2022/11/28.
//

#ifndef ACID_PERSISTER_H
#define ACID_PERSISTER_H

#include <optional>
#include <libgo/sync/co_mutex.h>
#include "../rpc/serializer.h"
#include "entry.h"
#include "snapshot.h"

namespace acid::raft {
// raft节点状态的持久化数据
struct HardState {
    // 当前任期
    int64_t term;
    // 任期内给谁投过票
    int64_t vote;
    // 已经commit的最大index
    int64_t commit;
    friend rpc::Serializer& operator<<(rpc::Serializer& s, const HardState& hs) {
        s << hs.term << hs.vote << hs.commit;
        return s;
    }
    friend rpc::Serializer& operator>>(rpc::Serializer& s, HardState& hs) {
        s >> hs.term >> hs.vote >> hs.commit;
        return s;
    }
};
/**
 * @brief 持久化存储
 */
class Persister {
public:
    using ptr = std::shared_ptr<Persister>;
    using MutexType = co::co_mutex;

    explicit Persister(const std::filesystem::path& persist_path = ".");

    ~Persister() = default;

    /**
     * @brief 获取持久化的 raft 状态
     */
    std::optional <HardState> loadHardState();
    /**
     * @brief 获取持久化的 log
     */
    std::optional <std::vector<Entry>> loadEntries();
    /**
     * @brief 获取快照
     */
    Snapshot::ptr loadSnapshot();
    /**
     * @brief 获取 raft state 的长度
     */
    int64_t getRaftStateSize();
    /**
     * @brief 持久化
     */
    bool persist(const HardState &hs, const std::vector <Entry> &ents, const Snapshot::ptr snapshot = nullptr);
    /**
     * @brief 获取快照路径
     */
    std::string getFullPathName() {
        return canonical(m_path);
    }
private:
    MutexType m_mutex;
    const std::filesystem::path m_path;
    Snapshotter m_shotter;
    const std::string m_name = "raft-state";
};
}
#endif //ACID_PERSISTER_H
