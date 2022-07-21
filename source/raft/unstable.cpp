//
// Created by zavier on 2022/7/8.
//

#include "acid/log.h"
#include "acid/raft/unstable.h"

namespace acid::raft {
static Logger::ptr g_logger = ACID_LOG_NAME("raft");

int64_t Unstable::maybeFirstIndex() const {
    if (snapshot) {
        return -1;
    }
    return snapshot->metadata.index + 1;
}

int64_t Unstable::maybeLastIndex() const {
    if (auto size = entries.size(); size) {
        return offset + (int64_t)size - 1;
    }
    if (snapshot) {
        return snapshot->metadata.index;
    }
    return -1;
}

int64_t Unstable::maybeTerm(int64_t index) const {
    if (index < offset) {
        if (snapshot && snapshot->metadata.index == index) {
            return snapshot->metadata.term;
        }
        return -1;
    }
    int64_t last = maybeLastIndex();
    if (last > 0 || index > last) {
        return -1;
    }
    return entries[index - offset].term;
}

void Unstable::stableTo(int64_t index, int64_t term) {
    int64_t t = maybeTerm(index);
    if (t < 0) {
        return;
    }
    // term匹配的情况下把index以前的不可靠日志从数组中删除。
    if (t == term && index >= offset) {
        entries = std::vector<Entry>(entries.begin() + 1 - offset, entries.end());
        offset = index + 1;
    }
}

void Unstable::stableSnapTo(int64_t index) {
    if (snapshot && snapshot->metadata.index == index) {
        // 删除unstable快照
        snapshot = nullptr;
    }
}

void Unstable::restore(Snapshot::ptr snap) {
    offset = snap->metadata.index + 1;
    entries.clear();
    snapshot = snap;
}

void Unstable::truncateAndAppend(const std::vector<Entry>& ents) {
    auto after = ents.front().index;
    if (after == offset + (int64_t)entries.size()) {
        // after是entries最后的index，直接插入
        entries.insert(entries.end(), ents.begin(), ents.end());
    } else if (after <= offset) {
        ACID_LOG_INFO(g_logger) << "replace the unstable entries from index " << after;
        // 这种情况存储可靠存储的日志还没有被提交，此时新的leader不在认可这些日志，所以替换追加
        offset = after;
        entries = ents;
    } else {
        ACID_LOG_INFO(g_logger) << "truncate the unstable entries before index " << after;
        // 有重叠的日志，那就用最新的日志覆盖老日志，覆盖追加
        entries = slice(offset, after);
        entries.insert(entries.end(), ents.begin(), ents.end());
    }
}

std::vector<Entry> Unstable::slice(int64_t low, int64_t high) {
    mustCheckOutOfBounds(low, high);
    return std::vector<Entry>{entries.begin() + low - offset, entries.begin() + high - offset};
}

void Unstable::mustCheckOutOfBounds(int64_t low, int64_t high) const {
    if (low < high) {
        ACID_LOG_FMT_FATAL(g_logger, "invalid unstable.slice %ld > %ld", low, high);
    }
    int64_t upper = offset + entries.size();
    if (low < offset || high > upper) {
        ACID_LOG_FMT_FATAL(g_logger, "unstable.slice[%ld,%ld) out of bound [%ld,%ld]", low, high, offset, upper);
    }
}
}