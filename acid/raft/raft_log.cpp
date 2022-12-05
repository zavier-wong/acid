//
// Created by zavier on 2022/7/11.
//

#include "raft_log.h"

namespace acid::raft {
static auto g_logger = GetLogInstance();

RaftLog::RaftLog(Persister::ptr persister, int64_t maxNextEntsSize /* = NO_LIMIT*/ ) {
    if (!persister) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "persister must not be nil");
        return;
    }

    auto opt = persister->loadEntries();
    if (opt) {
        m_entries = std::move(*opt);
        m_committed = persister->loadHardState()->commit;
        m_applied = firstIndex() - 1;
    } else {
        // 第一个为虚拟entry
        m_entries.emplace_back();
        m_committed = 0;
        m_applied = 0;
    }

    // 初始化为最后一次日志压缩的commit和apply
    m_maxNextEntriesSize = maxNextEntsSize;
}

int64_t RaftLog::maybeAppend(int64_t prevLogIndex, int64_t prevLogTerm, int64_t committed,
                    std::vector<Entry>& entries) {
    if (matchLog(prevLogIndex, prevLogTerm)) {
        int64_t last_index_of_new_entries = prevLogIndex + entries.size();
        int64_t conflict_index = findConflict(entries);
        if (conflict_index) {
            if (conflict_index <= m_committed) {
                SPDLOG_LOGGER_CRITICAL(g_logger, "entry {} conflict with committed entry [committed({})]", conflict_index, m_committed);
                exit(EXIT_FAILURE);
            } else {
                int64_t off = prevLogIndex + 1;
                if (conflict_index - off > (int)entries.size()) {
                    SPDLOG_LOGGER_CRITICAL(g_logger, "index {}, is out of range [{}]", conflict_index - off, entries.size());
                    exit(EXIT_FAILURE);
                }
                append(std::vector<Entry>{entries.begin() + conflict_index - off, entries.end()});
            }
        }
        return last_index_of_new_entries;
    }
    return -1;
}

int64_t RaftLog::append(const std::vector<Entry>& entries) {
    if (entries.empty()) {
        return lastIndex();
    }
    int64_t after = entries.front().index;
    // 不能覆盖已经 commit 的日志
    if (after - 1 < m_committed) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "after({}) is out of range [committed({})]", after, m_committed);
        exit(EXIT_FAILURE);
        return lastIndex();
    }
    if (after == lastIndex() + 1) {
        // after是entries最后的index，直接插入
        m_entries.insert(m_entries.end(), entries.begin(), entries.end());
    } else if (after <= m_entries.front().index) {
        SPDLOG_LOGGER_INFO(g_logger, "replace the entries from index {}", after);
        // 这种情况存储可靠存储的日志还没有被提交，此时新的leader不再认可这些日志，所以替换追加
        m_entries = entries;
    } else {
        SPDLOG_LOGGER_INFO(g_logger, "truncate the entries before index {}", after);
        // 有重叠的日志，那就用最新的日志覆盖老日志，覆盖追加
        auto offset = after - m_entries.front().index;
        m_entries.insert(m_entries.begin() + offset, entries.begin(), entries.end());
    }

    return lastIndex();
}

void RaftLog::append(const Entry& ent) {
    m_entries.push_back(ent);
}

int64_t RaftLog::findConflict(const std::vector<Entry>& entries) {
    for (const Entry& entry: entries) {
        if (!matchLog(entry.index, entry.term)) {
            if (entry.index <= lastIndex()) {
                SPDLOG_LOGGER_CRITICAL(g_logger, "found conflict at index {} [existing term: {}, conflicting term: {}]",
                    entry.index, term(entry.index), entry.term);
            }
            return entry.index;
        }
    }
    return 0;
}

int64_t RaftLog::findConflict(int64_t prevLogIndex, int64_t prevLogTerm) {
    int64_t last = lastIndex();
    if (prevLogIndex > last) {
         SPDLOG_LOGGER_TRACE(g_logger, "index({}) is out of range [0, lastIndex({})] in findConflict", prevLogIndex, last);
        return last + 1;
    }
    int64_t first = firstIndex();
    auto conflictTerm = term(prevLogIndex);
    int64_t index = prevLogIndex -1;
    // 尝试跳过一个 term
    while (index >= first && (term(index) == conflictTerm)) {
        --index;
    }
    return index;
}

std::vector<Entry> RaftLog::nextEntries() {
    int64_t off = std::max(m_applied + 1, firstIndex());
    if (m_committed + 1 > off) {
        return slice(off, m_committed + 1, m_maxNextEntriesSize);
    }
    return {};
}

bool RaftLog::hasNextEntries() {
    int64_t off = std::max(m_applied + 1, firstIndex());
    return m_committed + 1 > off;
}

void RaftLog::clearEntries(int64_t lastSnapshotIndex, int64_t lastSnapshotTerm) {
    m_entries.clear();
    m_entries.push_back({.index = lastSnapshotIndex, .term = lastSnapshotTerm});
}

int64_t RaftLog::firstIndex() {
    return m_entries.front().index + 1;
}

int64_t RaftLog::lastIndex() {
    return m_entries.front().index + (int64_t) m_entries.size() - 1;
}

void RaftLog::commitTo(int64_t commit) {
    if (m_committed < commit) {
        if (lastIndex() < commit) {
            SPDLOG_LOGGER_CRITICAL(g_logger, "commit({}) is out of range [lastIndex({})]. Was the raft log corrupted, truncated, or lost?",
               commit, lastIndex());
            return;
        }
        m_committed = commit;
    }
}

void RaftLog::appliedTo(int64_t index) {
    if (!index) {
        return;
    }
    if (m_committed < index || index < m_applied) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "applied({}) is out of range [prevApplied({}), committed({})]", index, m_applied, m_committed);
        return;
    }
    m_applied = index;
}


int64_t RaftLog::lastTerm() {
    int64_t t = term(lastIndex());
    if (t < 0) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "unexpected error when getting the last term");
    }
    return t;
}

int64_t RaftLog::lastSnapshotIndex() {
    return m_entries.front().index;
}

int64_t RaftLog::lastSnapshotTerm() {
    return m_entries.front().term;
}

int64_t RaftLog::term(int64_t index) {
    int64_t offset = m_entries.front().index;
    if (index < offset) {
        return -1;
    }
    if (index - offset >= (int64_t) m_entries.size()) {
        return -1;
    }
    return m_entries[index - offset].term;
}

std::vector<Entry> RaftLog::entries(int64_t index) {
    // 发送心跳
    if (index > lastIndex()) {
        return {};
    }
    return slice(index, lastIndex() + 1, m_maxNextEntriesSize);

}

std::vector<Entry> RaftLog::allEntries() {
    return m_entries;
}

bool RaftLog::isUpToDate(int64_t index, int64_t term) {
    return term > lastTerm() || (term == lastTerm() && index >= lastIndex());
}

bool RaftLog::matchLog(int64_t index, int64_t term) {
    int64_t t = this->term(index);
    if (t < 0) {
        return false;
    }
    return t == term;
}

bool RaftLog::maybeCommit(int64_t maxIndex, int64_t term) {
    // 只有领导人当前任期里的日志条目通过计算副本数目可以被提交
    if (maxIndex > m_committed && this->term(maxIndex) == term) {
        commitTo(maxIndex);
        return true;
    }
    return false;
}

std::vector<Entry> RaftLog::slice(int64_t low, int64_t high, int64_t maxSize) {
    mustCheckOutOfBounds(low, high - 1);
    if (maxSize != NO_LIMIT) {
        high = std::min(high, low + maxSize);
    }

    // 计算实际下标
    low -= lastSnapshotIndex();
    high -= lastSnapshotIndex();

    return std::vector<Entry>{m_entries.begin() + low, m_entries.begin() + high};
}

void RaftLog::mustCheckOutOfBounds(int64_t low, int64_t high) {
    if (low > high) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "invalid slice {} > {}", low, high);
        exit(EXIT_FAILURE);
    }
    if (low < firstIndex() || high > lastIndex()) {
        SPDLOG_LOGGER_ERROR(g_logger, "slice[{},{}) out of bound [{},{}]", low, high, firstIndex(), lastIndex());
        exit(EXIT_FAILURE);
    }
}

Snapshot::ptr RaftLog::createSnapshot(int64_t index, const std::string &data) {
    if (index <= m_entries.front().index) {
        return nullptr;
    }
    if (index > m_committed) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "index > m_committed");
        return nullptr;
    }
    int64_t offset = m_entries.front().index;
    if (index > lastIndex()) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "snapshot {} is out of bound last index({})", index, lastIndex());
        return nullptr;
    }
    Snapshot::ptr snap = std::make_shared<Snapshot>();
    snap->metadata.index = index;
    snap->metadata.term = m_entries[index - offset].term;
    snap->data = data;
    SPDLOG_LOGGER_DEBUG(g_logger, "log [{}] starts to restore snapshot [index: {}, term: {}]",
                           toString(), snap->metadata.index, snap->metadata.term);
    return snap;
}

bool RaftLog::compact(int64_t compactIndex) {
    int64_t offset = m_entries.front().index;
    if (compactIndex <= offset) {
        return false;
    }
    if (compactIndex > lastIndex()) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "snapshot {} is out of bound last index({})", compactIndex, lastIndex());
        return false;
    }
    int64_t index = compactIndex - offset;
    m_entries.erase(m_entries.begin(), m_entries.begin() + index);
    m_entries[0].data = "";
    return true;
}

}