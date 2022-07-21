//
// Created by zavier on 2022/7/8.
//
#include "acid/macro.h"
#include "acid/raft/storage.h"

namespace acid::raft {
static Logger::ptr g_logger = ACID_LOG_NAME("raft");

MemoryStorage::MemoryStorage() {
    // 在第 0 项处使用虚拟条目填充
    Entry entry;
    entry.type = Entry::Dummy;
    m_entries.push_back(entry);
}

std::optional<HardState> MemoryStorage::initialState() {
    return m_hardState;
}

void MemoryStorage::setHardState(HardState hs) {
    MutexType::Lock lock(m_mutex);
    m_hardState = hs;
}

std::pair<std::vector<Entry>, StorageError> MemoryStorage::entries(int64_t low, int64_t high, int64_t maxSize) {
    MutexType::Lock lock(m_mutex);
    int64_t offset = m_entries.front().index;
    if (low < offset) {
        return {{}, StorageError::Compacted};
    }
    if (high > LastIndex() + 1) {
        ACID_LOG_FMT_FATAL(g_logger, "entries high(%ld) is out of bound last index(%ld)", high, LastIndex());
    }
    // 只有dummy日志
    if (m_entries.size() == 1) {
        return {{}, StorageError::Unavailable};
    }

    auto beg = m_entries.begin() + low - offset;
    auto end = m_entries.begin() + std::min(high - offset, low - offset + maxSize);

    return {std::vector<Entry>{beg, end}, StorageError::Succeed};
}

int64_t MemoryStorage::term(int64_t index) {
    MutexType::Lock lock(m_mutex);
    int64_t offset = m_entries.front().index;
    if (index < offset) {
        return StorageError::Compacted;
    }
    if (index - offset >= (int64_t)m_entries.size()) {
        return StorageError::Unavailable;
    }
    return m_entries[index - offset].term;
}

int64_t MemoryStorage::lastIndex() {
    MutexType::Lock lock(m_mutex);
    return LastIndex();
}

int64_t MemoryStorage::LastIndex() {
    return m_entries.front().index + (int64_t)m_entries.size() - 1;
}

int64_t MemoryStorage::firstIndex() {
    MutexType::Lock lock(m_mutex);
    return FirstIndex();
}

int64_t MemoryStorage::FirstIndex() {
    return m_entries.front().index + 1;
}

Snapshot::ptr MemoryStorage::snapshot() {
    MutexType::Lock lock(m_mutex);
    return m_snapshot;
}

    StorageError MemoryStorage::applySnapshot(Snapshot::ptr snap) {
    MutexType::Lock lock(m_mutex);
    int64_t index = m_snapshot->metadata.index;
    int64_t snapIndex = snap->metadata.index;
    if (index >= snapIndex) {
        return StorageError::SnapOutOfDate;
    }
    m_snapshot = snap;
    m_entries.clear();
    m_entries.push_back({snap->metadata.index, snap->metadata.term});
    return StorageError::Succeed;
}

Snapshot::ptr MemoryStorage::CreateSnapshot(int64_t index, const std::string& data) {
    MutexType::Lock lock(m_mutex);
    if (index < m_snapshot->metadata.index) {
        return nullptr;
    }
    int64_t offset = m_entries.front().index;
    if (index > LastIndex()) {
        ACID_LOG_FMT_FATAL(g_logger, "snapshot %ld is out of bound last index(%ld)", index, LastIndex());
    }
    m_snapshot->metadata.index = index;
    m_snapshot->metadata.term = m_entries[index - offset].term;
    m_snapshot->data = data;
    return m_snapshot;
}

StorageError MemoryStorage::compact(int64_t compactIndex) {
    MutexType::Lock lock(m_mutex);
    int64_t offset = m_entries.front().index;
    if (compactIndex <= offset) {
        return StorageError::Compacted;
    }
    if (compactIndex > LastIndex()) {
        ACID_LOG_FMT_FATAL(g_logger, "snapshot %ld is out of bound last index(%ld)", compactIndex, LastIndex());
    }
    int64_t index = compactIndex - offset;
    std::vector<Entry> ents;
    ents.reserve(m_entries.size() - index + 1);
    ents.front().index = m_entries[index].index;
    ents.front().term = m_entries[index].term;
    ents.insert(ents.cend(), m_entries.begin() + index + 1, m_entries.end());
    m_entries = ents;
    return StorageError::Succeed;
}

// entries[0].Index > ms.entries[0].Index
void MemoryStorage::append(std::vector<Entry> ents) {
    if (ents.empty()) {
        return;
    }
    MutexType::Lock lock(m_mutex);
    int64_t first = FirstIndex();
    int64_t last = ents.front().index + ents.size() - 1;

    if (last < first) {
        return;
    }

    if (first > ents.front().index) {
        ents = std::vector<Entry>{ents.begin() + first - ents.front().index, ents.end()};
    }
    int64_t offset = ents.front().index - m_entries.front().index;
    if ((int64_t)m_entries.size() > offset) {
        m_entries.insert(m_entries.cbegin() + offset, ents.begin(), ents.end());
    } else if ((int64_t)m_entries.size() == offset) {
        m_entries.insert(m_entries.cend(), ents.begin(), ents.end());
    } else {
        ACID_LOG_FMT_FATAL(g_logger,
           "missing log entry [last: %ld, append at: %ld]", LastIndex(), ents.front().index);
    }
}

}