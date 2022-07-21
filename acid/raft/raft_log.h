//
// Created by zavier on 2022/7/5.
//

#ifndef ACID_RAFT_LOG_H
#define ACID_RAFT_LOG_H


#include <string>
#include "../rpc/serializer.h"
#include "entry.h"
#include "snapshot.h"
#include "storage.h"
#include "unstable.h"

namespace acid::raft {
/**
 * @brief raftLog记录着日志。
 * @details raftLog结构体中，既有还未持久化的数据，也有已经持久化到稳定存储的数据；其中数据既有日志条目，也有快照
 */
class RaftLog {
public:
    using ptr = std::shared_ptr<RaftLog>;
    static constexpr int64_t NO_LIMIT = INT64_MAX;
    RaftLog(Storage::ptr storage, int64_t maxNextEntsSize = NO_LIMIT);
    /**
     * @brief 追加日志，在收到leader追加日志的消息后被调用
     * @details 为什么是maybe？更确切的说什么原因会失败？这就要从index,term这两个参数说起了。
     * raft会把若干个日志条目(Entry)封装在一个请求(Request)中，同时在消息中还有prevLogIndex和
     * prevLogTerm两个参数。这两个参数是entries前一条日志的index和term，此处只需要知道一点，
     * leader有一个参数记录下一次将要发送给某个节点的索引起始值，也就是entries[0].index，
     * 而index和term值就是entries[-1].index和entries[-1].term。
     *
     */
    int64_t maybeAppend(int64_t prevLogIndex, int64_t prevLogTerm, int64_t committed,
                        std::vector<Entry>& entries);
    // 追加日志
    int64_t append(const std::vector<Entry>& entries);
    // 找到冲突日志的起始index
    // 如果没有冲突的日志，返回0
    // 如果没有冲突的日志，entries 包含新的日志，返回第一个新日志的index
    // 一条日志如果有一样的index但是term不一样，认为是冲突的日志
    // 给定条目的index必须不断增加
    int64_t findConflict(const std::vector<Entry>& entries);
    // 获取不可靠日志，就是把unstable的所有日志输出，这个函数用于输出给使用者持久化
    std::vector<Entry> unstableEntries();
    // 获取apply索引到commit索引间的所有日志，这个函数用于输出给使用者apply日志
    //
    std::vector<Entry> nextEntries();

    // 判断是否有可应用的日志
    bool hasNextEntries();

    // 获取快照
    Snapshot::ptr snapshot();
    // 获取第一个索引，你可能会问第一个日志索引不应该是0或者是1么？(取决于索引初始值)，但是raft会
    // 周期的做快照，快照之前的日志就没用了，所以第一个日志索引不一定是0
    int64_t firstIndex();

    // 获取最后一条日志的索引。
    int64_t lastIndex();

    // 更新提交索引
    void commitTo(int64_t tocommit);

    // 更新应用索引
    void appliedTo(int64_t index);
    // 使用者告知raftLog日志已经持久化到哪个索引了
    void stableTo(int64_t index, int64_t term);
    // 使用者告知raftLog索引值为i的快照已经持久化了
    void stableSnapTo(int64_t index);

    int64_t lastTerm();

    // 获取指定索引日志的term
    int64_t term(int64_t index);

    // 获取从索引值为i之后的所有日志，但是日志总量限制在maxsize
    std::pair<std::vector<Entry>, StorageError> entries(int64_t index, int64_t maxSize);
    // 获取所有日志
    std::vector<Entry> allEntries();
    // 判断给定日志的索引和任期是不是比raftLog中的新
    // Raft 通过比较两份日志中最后一条日志条目的索引值和任期号定义谁的日志比较新。
    // 如果两份日志最后的条目的任期号不同，那么任期号大的日志更加新。
    // 如果两份日志最后的条目任期号相同，那么日志比较长的那个就更加新。
    bool isUpToDate(int64_t lasti, int64_t term);
    // 判断给定日志的索引和任期是不是和自己的一样
    bool matchLog(int64_t index, int64_t term);
    // 更新提交索引
    // Raft 永远不会通过计算副本数目的方式去提交一个之前任期内的日志条目。
    // 只有领导人当前任期里的日志条目通过计算副本数目可以被提交
    bool maybeCommit(int64_t maxIndex, int64_t term);
    // 保存快照
    void restore(Snapshot::ptr s);

    // 获取(lo，hi]的所有日志，但是总量限制在maxSize
    std::pair<std::vector<Entry>, StorageError> slice(int64_t low, int64_t high, int64_t maxSize);

    StorageError mustCheckOutOfBounds(int64_t low, int64_t high);
    int64_t zeroTermOnErrCompacted(int64_t term);

    int64_t getCommitted() const { return m_committed;}
    int64_t getApplied() const { return m_applied;}
    std::string toString() const {
        std::string str = "{ committed = " + std::to_string(m_committed) +
                          ", applied = " + std::to_string(m_applied) +
                          ", unstable.offset = " + std::to_string(m_unstable.offset) +
                          ", unstable.entries.size() = " + std::to_string(m_unstable.entries.size()) +
                          " }";
        return str;
    }

private:
    // storage存储了从最后一次snapshot到现在的所有可靠的（stable）日志（Entry），虽然是用来
    // 存储日志的，但是这个存储没有持久化能力，日志的持久化相关的处理raft并不关心，这个存储可以
    // 理解为日志的缓存，raft访问最近的日志都是通过他。也就是说storage中的日志在使用者的持久化
    // 存储中也有一份，当raft需要访问这些日志的时候，无需访问持久化的存储，不仅效率高，而且与持
    // 久化存储充分解耦。那storage中缓存了多少日志呢？从当前到上一次快照之间的所有日志(entry)
    // 以及上一次快照都在storage中。这个可以从Storage的MemoryStorage实现可以看出来
    Storage::ptr m_storage;
    // 与storage形成了鲜明的对比，那些还没有被使用者持久化的日志存储在unstable中，如果说storage
    // 是持久化存储日志的cache，也就是说这些日志因为持久化了所以变得可靠，但是日志持久化需要时间，
    // 并且是raft通过异步的方式把需要持久化的日志输出给使用者。在使用者没有通知raft日志持久化完毕前，
    // 这些日志都还不可靠，需要用unstable来管理。当使用者持久化完毕后，这些日志就会从unstable删除，
    // 并记录在storage中。
    Unstable m_unstable;
    // 已知已提交的最高的日志条目的索引（初始值为0，单调递增）
    // 被超过半数以上peer的RaftLog.storage存储的日志中的最大索引值。该值记录者当前节点已知的
    // 提交索引，需要注意，提交索引是集群的一个状态，而不是某一节点的状态，但是leader在广播提交
    // 索引的时候会因为多种原因造成到达速度不一，所以每个节点知道的提交索引可能不同。
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
