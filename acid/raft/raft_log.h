//
// Created by zavier on 2022/7/5.
//

#ifndef ACID_RAFT_LOG_H
#define ACID_RAFT_LOG_H


#include <string>
#include "../rpc/serializer.h"
#include "entry.h"
#include "snapshot.h"
#include "persister.h"

namespace acid::raft {
/**
 * @brief raftLog记录着日志，封装了一些和日志有关的操作
 */
class RaftLog {
public:
    using ptr = std::shared_ptr<RaftLog>;
    static constexpr int64_t NO_LIMIT = INT64_MAX;
    RaftLog(Persister::ptr persister, int64_t maxNextEntsSize = NO_LIMIT);
    /**
     * @brief 追加日志，在收到leader追加日志的消息后被调用
     */
    int64_t maybeAppend(int64_t prevLogIndex, int64_t prevLogTerm, int64_t committed,
                        std::vector<Entry>& entries);
    /**
     * @brief 追加日志
     */
    int64_t append(const std::vector<Entry>& entries);
    /**
     * @brief 追加日志
     */
    void append(const Entry& ent);
    /**
     * @brief 从 entries 里找到冲突的日志
     * @details 如果没有冲突的日志，返回0;
     * 如果没有冲突的日志，entries 包含新的日志，返回第一个新日志的index;
     * 一条日志如果有一样的index但是term不一样，认为是冲突的日志;
     * 给定条目的index必须不断增加;
     */
    int64_t findConflict(const std::vector<Entry>& entries);
    /**
     * @brief 找出冲突任期的第一条日志索引
     */
    int64_t findConflict(int64_t prevLogIndex, int64_t prevLogTerm);
    /**
     * @brief 获取[apply,commit]间的所有日志，这个函数用于输出给使用者apply日志
     */
    std::vector<Entry> nextEntries();
    /**
     * @brief 判断是否有可应用的日志
     */
    bool hasNextEntries();
    /**
     * @brief 获取第一个索引
     * @note 你可能会问第一个日志索引不应该是0么，但是raft会周期的做快照，快照之前的日志就没用了，所以第一个日志索引不一定是0
     */
    int64_t firstIndex();
    /**
     * @brief 获取最后一条日志的索引。
     */
    int64_t lastIndex();
    /**
     * @brief 获取最后一条日志的任期。
     */
    int64_t lastTerm();
    /**
     * @brief 获取快照之前的一条日志的 index
     */
    int64_t lastSnapshotIndex();
    /**
     * @brief 获取快照之前的一条日志的 term
     */
    int64_t lastSnapshotTerm();
    /**
     * @brief 更新 commit 到 commit
     */
    void commitTo(int64_t commit);
    /**
     * @brief 更新 applied 到 index
     */
    void appliedTo(int64_t index);
    /**
     * @brief 获取指定索引日志的term
     */
    int64_t term(int64_t index);
    /**
     * @brief 获取[index，lastIndex()]的所有日志，但是日志总量限制在maxsize
     */
    std::vector<Entry> entries(int64_t index, int64_t maxSize);
    /**
     * @brief 获取所有日志
     */
    std::vector<Entry> allEntries();
    /**
     * @brief 判断给定日志的索引和任期是不是比自己新
     * @details Raft 通过比较两份日志中最后一条日志条目的索引值和任期号定义谁的日志比较新。
     * 如果两份日志最后的条目的任期号不同，那么任期号大的日志更加新。
     * 如果两份日志最后的条目任期号相同，那么日志比较长的那个就更加新。
     */
    bool isUpToDate(int64_t index, int64_t term);
    /**
     * @brief 判断给定日志的索引和任期是否正确
     */
    bool matchLog(int64_t index, int64_t term);
    /**
     * @brief Leader更新 commit index
     * @note Raft 永远不会通过计算副本数目的方式去提交一个之前任期内的日志条目。只有领导人当前任期里的日志条目通过计算副本数目可以被提交
     */
    bool maybeCommit(int64_t maxIndex, int64_t term);
    /**
     * @brief 获取[low，high)的所有日志，但是总量限制在maxSize
     */
    std::vector<Entry> slice(int64_t low, int64_t high, int64_t maxSize);
    /**
     * @brief 检查 index 是否正确，
     * @note firstIndex <= low <= high <= lastIndex
     */
    void mustCheckOutOfBounds(int64_t low, int64_t high);
    /**
     * @brief 创建快照并压缩 index 之前的日志
     */
    Snapshot::ptr createSnapshot(int64_t index, const std::string &data);
    /**
     * @brief 压缩日志
     */
    bool compact(int64_t compactIndex);

    [[nodiscard]]
    int64_t committed() const { return m_committed;}
    [[nodiscard]]
    int64_t applied() const { return m_applied;}
    [[nodiscard]]
    std::string toString() const {
        std::string str = fmt::format("committed: {}, applied: {}, offset: {}, length: {}",
                                      m_committed, m_applied, m_entries.front().index, m_entries.size());
        return str;
    }

private:
    // 日志
    std::vector<Entry> m_entries;
    // commit
    int64_t m_committed;
    // 已经被应用到状态机的最高的日志条目的索引（初始值为0，单调递增）
    // 前文就提到了apply这个词，日志的有序存储并不是目的，日志的有序apply才是目的，已经提交的日志
    // 就要被使用者apply，apply就是把日志数据反序列化后在raft状态机上执行。applied 就是该节点已
    // 经被apply的最大索引。apply索引是节点状态(非集群状态)，这取决于每个节点的apply速度。
    int64_t m_applied;
    // raftLog有一个成员函数nextEntries(),用于获取 (applied,committed] 的所有日志，很容易看出来
    // 这个函数是apply日志时调用的，maxNextEntriesSize就是用来限制获取日志大小总量的，避免一次调用
    // 产生过大粒度的apply操作。
    int64_t m_maxNextEntriesSize;
};

}
#endif //ACID_RAFT_LOG_H
