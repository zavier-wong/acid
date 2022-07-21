//
// Created by zavier on 2022/7/11.
//

#include "acid/raft/raft_log.h"

namespace acid::raft {
static Logger::ptr g_logger = ACID_LOG_NAME("raft");

RaftLog::RaftLog(Storage::ptr storage, int64_t maxNextEntsSize /* = NO_LIMIT*/ ) {
    if (!storage) {
        ACID_LOG_FATAL(g_logger) << "storage must not be nil";
    }
    m_storage = storage;
    int64_t first_index = storage->firstIndex();
    int64_t last_index = storage->lastIndex();

    if (first_index < 0 || last_index < 0) {
        ACID_LOG_FATAL(g_logger) << "get index error";
    }
    // 初始化为最后一次日志压缩的commit和apply
    m_unstable.offset = last_index + 1;
    m_committed = first_index - 1;
    m_applied = first_index - 1;
    m_maxNextEntriesSize = maxNextEntsSize;
}

int64_t RaftLog::maybeAppend(int64_t prevLogIndex, int64_t prevLogTerm, int64_t committed,
                    std::vector<Entry>& entries) {
    if (matchLog(prevLogIndex, prevLogTerm)) {
        int64_t last_index_of_new_entries = prevLogIndex + entries.size();
        int64_t conflict_index = findConflict(entries);
        if (conflict_index < 0 || conflict_index <= m_committed) {
            ACID_LOG_FMT_FATAL(g_logger,
               "entry %ld conflict with committed entry [committed(%ld)]",
               conflict_index, m_committed);
        } else {
            int64_t offset = prevLogIndex + 1;

            if (conflict_index - offset > (int64_t)entries.size()) {
                ACID_LOG_FMT_FATAL(g_logger,
                   "index, %d, is out of range [%d]",
                   conflict_index - offset, entries.size());
            }
            append(std::vector<Entry>{entries.begin() + conflict_index - offset, entries.end()});
        }
        // 更新提交索引，为什么取了个最小？committed是leader发来的，是全局状态，但是当前节点
        // 可能落后于全局状态，所以取了最小值。last_index_of_new_entries是这个节点最新的索引，
        // 不是大的可靠索引，如果此时节点异常了，会不会出现提交索引以前的日志已经被应用，但是有
        // 些日志还没有被持久化？这里需要解释一下，raft更新了提交索引，raft会把提交索引以前的
        // 日志交给使用者应用同时会把不可靠日志也交给使用者持久化，所以这要求使用者必须先持久化日志
        // 再应用日志，否则就会出现刚刚提到的问题。
        commitTo(std::min(committed, last_index_of_new_entries));
        return last_index_of_new_entries;
    }
    return -1;
}

int64_t RaftLog::append(const std::vector<Entry>& entries) {
    if (entries.empty()) {
        return lastIndex();
    }
    int64_t after = entries.front().index - 1;
    if (after < m_committed) {
        ACID_LOG_FMT_FATAL(g_logger, "after(%d) is out of range [committed(%d)]", after, m_committed);
    }
    m_unstable.truncateAndAppend(entries);
    return lastIndex();
}

int64_t RaftLog::findConflict(const std::vector<Entry>& entries) {
    for (const Entry& entry: entries) {
        if (!matchLog(entry.index, entry.term)) {
            if (entry.index < lastIndex()) {
                ACID_LOG_FMT_INFO(g_logger,
                  "found conflict at index %d [existing term: %d, conflicting term: %d]",
                  entry.index, zeroTermOnErrCompacted(term(entry.index)), entry.term);
            }
            return entry.index;
        }
    }
    return -1;
}

std::vector<Entry> RaftLog::unstableEntries() {
    return m_unstable.entries;
}

std::vector<Entry> RaftLog::nextEntries() {
    int64_t off = std::max(m_applied + 1, firstIndex());
    if (m_committed + 1 > off) {
        auto [ents, err] = slice(off, m_committed + 1, m_maxNextEntriesSize);
        if (err) {
            ACID_LOG_FATAL(g_logger) << "unexpected error when getting unapplied entries";
        }
        return ents;
    }
    return {};
}

bool RaftLog::hasNextEntries() {
    int64_t off = std::max(m_applied + 1, firstIndex());
    return m_committed + 1 > off;
}

Snapshot::ptr RaftLog::snapshot() {
    if (m_unstable.snapshot) {
        return m_unstable.snapshot;
    }
    return m_storage->snapshot();
}

int64_t RaftLog::firstIndex() {
    int64_t index = m_unstable.maybeFirstIndex();
    if (index > 0) {
        return index;
    }
    index = m_storage->firstIndex();
    if (index < 0) {
        ACID_LOG_FATAL(g_logger) << "unexpected error when getting first index";
    }
    return index;
}

int64_t RaftLog::lastIndex() {
    int64_t index = m_unstable.maybeLastIndex();
    if (index > 0) {
        return index;
    }
    index = m_storage->lastIndex();
    if (index < 0) {
        ACID_LOG_FATAL(g_logger) << "unexpected error when getting last index";
    }
    return index;
}

void RaftLog::commitTo(int64_t tocommit) {
    if (m_committed < tocommit) {
        if (lastIndex() < tocommit) {
            ACID_LOG_FMT_FATAL(g_logger,
               "tocommit(%ld) is out of range [lastIndex(%ld)]. Was the raft log corrupted, truncated, or lost?",
               tocommit, lastIndex());
        }
        m_committed = tocommit;
    }
}

void RaftLog::appliedTo(int64_t index) {
    if (!index) {
        return;
    }
    if (m_committed < index || index < m_applied) {
        ACID_LOG_FMT_FATAL(g_logger,
           "applied(%ld) is out of range [prevApplied(%ld), committed(%ld)]",
           index, m_applied, m_committed);
    }
    m_applied = index;
}

void RaftLog::stableTo(int64_t index, int64_t term) {
    m_unstable.stableTo(index, term);
}

void RaftLog::stableSnapTo(int64_t index) {
    m_unstable.stableSnapTo(index);
}

int64_t RaftLog::lastTerm() {
    int64_t t = term(lastIndex());
    if (t < 0) {
        ACID_LOG_FATAL(g_logger) << "unexpected error when getting the last term";
    }
    return t;
}

int64_t RaftLog::term(int64_t index) {
    int64_t dummyIndex = lastIndex() - 1;
    if (index < dummyIndex || index > lastIndex()) {
        return -1;
    }
    int64_t t = m_unstable.maybeTerm(index);
    if (t >= 0) {
        return t;
    }
    t = m_storage->term(index);
    if (t >= 0 || t == StorageError::Compacted || t == StorageError::Unavailable) {
        return t;
    }
    ACID_LOG_FATAL(g_logger) << "unexpected error when getting term";
    return 0;
}

std::pair<std::vector<Entry>, StorageError> RaftLog::entries(int64_t index, int64_t maxSize) {
    if (index > lastIndex()) {
        return {{}, StorageError::Succeed};
    }
    return slice(index, lastIndex() + 1, maxSize);
}

std::vector<Entry> RaftLog::allEntries() {
    auto [ents, err] = entries(firstIndex(), NO_LIMIT);
    if (!err) {
        return ents;
    }
    if (err == StorageError::Compacted) {
        return allEntries();
    }
    ACID_LOG_FATAL(g_logger) << "unexpected error when getting allEntries";
    return {};
}

bool RaftLog::isUpToDate(int64_t lasti, int64_t term) {
    return term > lastTerm() || (term == lastTerm() && lasti >= lastIndex());
}

bool RaftLog::matchLog(int64_t index, int64_t term) {
    int64_t t = this->term(index);
    if (t < 0) {
        return false;
    }
    return t == term;
}

bool RaftLog::maybeCommit(int64_t maxIndex, int64_t term) {
    if (maxIndex > m_committed && zeroTermOnErrCompacted(this->term(maxIndex)) == term) {
        commitTo(maxIndex);
        return true;
    }
    return false;
}

void RaftLog::restore(Snapshot::ptr s) {
    ACID_LOG_FMT_DEBUG(g_logger,
       "log [%s] starts to restore snapshot [index: %ld, term: %ld]",
       toString().c_str(), s->metadata.index, s->metadata.term);
    m_committed = s->metadata.index;
    m_unstable.restore(s);
}

std::pair<std::vector<Entry>, StorageError> RaftLog::slice(int64_t low, int64_t high, int64_t maxSize) {
    auto check = mustCheckOutOfBounds(low, high);
    if (check) {
        return {{}, check};
    }
    if (low == high) {
        return {{}, StorageError::Succeed};
    }
    std::vector<Entry> ents;
    if (low < m_unstable.offset) {
        auto [storedEnts, err] = m_storage->entries(low, std::min(high, m_unstable.offset), maxSize);
        if (err == StorageError::Compacted) {
            return {{}, err};
        } else if (err == StorageError::Unavailable) {
            ACID_LOG_FATAL(g_logger) << "entries[" << low << ":"
                << std::min(high, m_unstable.offset) << ") is unavailable from storage";
        } else if (err) {
            ACID_LOG_FATAL(g_logger) << "unexpected error when slice";
        }
        if ((int64_t)storedEnts.size() < std::min(high, m_unstable.offset) - low) {
            return {storedEnts, StorageError::Succeed};
        }
        ents = storedEnts;
    }
    if (high > m_unstable.offset) {
        auto unstable = m_unstable.slice(std::max(low, m_unstable.offset), high);
        if (ents.size()) {
            ents.insert(ents.end(), unstable.cbegin(), unstable.cend());
        } else {
            ents = unstable;
        }
    }
    if ((int64_t)ents.size() > maxSize) {
        ents.erase(ents.begin() + maxSize, ents.end());
    }
    return {ents, StorageError::Succeed};
}

StorageError RaftLog::mustCheckOutOfBounds(int64_t low, int64_t high) {
    if (low > high) {
        ACID_LOG_FATAL(g_logger) << "invalid slice " << low << " > " << high;
    }
    int64_t first = firstIndex();
    if (low < first) {
        return StorageError::Compacted;
    }
    int64_t length = lastIndex() + 1 - first;
    if (high > first + length) {
        ACID_LOG_FMT_FATAL(g_logger,
           "slice[%ld,%ld) out of bound [%ld,%ld]",
           low, high, first, lastIndex());
    }
    return StorageError::Succeed;
}

int64_t RaftLog::zeroTermOnErrCompacted(int64_t term) {
    if (term >= 0) {
        return term;
    }
    if (term == StorageError::Compacted) {
        return 0;
    }
    ACID_LOG_FATAL(g_logger) << "unexpected error " << term;
    return 0;
}

// RaftLog::
}