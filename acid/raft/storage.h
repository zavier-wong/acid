//
// Created by zavier on 2022/7/8.
//

#ifndef ACID_STORAGE_H
#define ACID_STORAGE_H
#include <string>
#include <optional>
#include <libgo/libgo.h>
#include "../rpc/serializer.h"
#include "entry.h"
#include "snapshot.h"

namespace acid::raft {

enum StorageError {
    Succeed = 0,
    Compacted = -1,
    SnapOutOfDate = -2,
    Unavailable = 3,
    SnapshotTemporarilyUnavailable = -4,
};


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
 * @brief Storage是一个抽象类
 */
class Storage {
public:
    using ptr = std::shared_ptr<Storage>;
    virtual std::optional<HardState> initialState() = 0;
    virtual std::pair<std::vector<Entry>, StorageError> entries(int64_t low, int64_t high, int64_t maxSize) = 0;
    virtual int64_t term(int64_t index) = 0;
    virtual int64_t lastIndex() = 0;
    virtual int64_t firstIndex() = 0;
    virtual Snapshot::ptr snapshot() = 0;
    virtual ~Storage() = default;
};

class MemoryStorage : public Storage {
public:
    using MutexType = co::co_mutex;
    MemoryStorage();
    ~MemoryStorage() = default;

    std::optional<HardState> initialState() override;

    void setHardState(HardState hs);

    std::pair<std::vector<Entry>, StorageError> entries(int64_t low, int64_t high, int64_t maxSize) override;

    int64_t term(int64_t index) override;

    int64_t lastIndex() override;

    int64_t firstIndex() override;

    Snapshot::ptr snapshot() override;

    StorageError applySnapshot(Snapshot::ptr snap);

    Snapshot::ptr CreateSnapshot(int64_t index, const std::string& data);

    StorageError compact(int64_t compactIndex);

    void append(std::vector<Entry> ents);

private:
    // 无锁方法，内部使用

    int64_t LastIndex();

    int64_t FirstIndex();

private:
    MutexType m_mutex;
    HardState m_hardState;
    Snapshot::ptr m_snapshot;
    std::vector<Entry> m_entries;
};


}
#endif //ACID_STORAGE_H
