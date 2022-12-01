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

class Persister {
public:
    using ptr = std::shared_ptr<Persister>;
    using MutexType = co::co_mutex;

    explicit Persister(const std::filesystem::path& persist_path = ".");

    ~Persister() = default;

    std::optional <HardState> loadHardState();

    std::optional <std::vector<Entry>> loadEntries();

    Snapshot::ptr loadSnapshot();

    bool persist(const HardState &hs, const std::vector <Entry> &ents, const Snapshot::ptr snapshot = nullptr);

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
