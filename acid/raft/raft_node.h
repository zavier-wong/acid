//
// Created by zavier on 2022/6/13.
//

#ifndef ACID_RAFT_NODE_H
#define ACID_RAFT_NODE_H

#include <vector>
#include "../rpc/rpc_server.h"
#include "raft_log.h"
#include "raft_peer.h"
#include "snapshot.h"

namespace acid::raft {
using namespace acid::rpc;
/**
 * @brief Raft 的状态
 */
enum RaftState {
    Follower,   // 追随者
    Candidate,  // 候选人
    Leader      // 领导者
};

struct ApplyMsg {
    enum MsgType {
        ENTRY,      // 日志
        SNAPSHOT,   // 快照
    };
    ApplyMsg() = default;
    explicit ApplyMsg(const Entry& ent) : type(ENTRY), data(ent.data), index(ent.index), term(ent.term) {}
    explicit ApplyMsg(const Snapshot& snap) : type(SNAPSHOT), data(snap.data), index(snap.metadata.index), term(snap.metadata.term) {}
    MsgType type = ENTRY;
    std::string data{};
    int64_t index{};
    int64_t term{};
};

/**
 * @brief Raft 节点，处理 rpc 请求，并改变状态，通过 RaftPeer 调用远端 Raft 节点
 */
class RaftNode : public acid::rpc::RpcServer {
public:
    using ptr = std::shared_ptr<RaftNode>;
    using MutexType = co::co_mutex;

    /**
     * Create a Raft server
     * @param id 当前 raft 节点在集群内的唯一标识
     * @param persister raft 保存其持久状态的地方，并且从保存的状态初始化当前节点
     * @param applyChan 是发送达成共识的日志的 channel
     */
    RaftNode(int64_t id, Persister::ptr persister, co::co_chan<ApplyMsg> applyChan);

    ~RaftNode();
    /**
     * @brief 启动 raft
     */
    void start() override;
    /**
     * @brief 增加 raft 节点
     * @param[in] id raft 节点id
     * @param[in] address raft 节点地址
     */
    void addPeer(int64_t id, Address::ptr address);

    bool isLeader();
    /**
     * @brief 发起一条消息
     * @return 如果该节点不是 Leader 返回 std::nullopt
     */
    std::optional<Entry> propose(const std::string& data);
    /**
     * @brief 处理远端 raft 节点的投票请求
     */
    RequestVoteReply handleRequestVote(RequestVoteArgs request);
    /**
     * @brief 处理远端 raft 节点的日志追加请求
     */
    AppendEntriesReply handleAppendEntries(AppendEntriesArgs request);
    /**
     * @brief 处理远端 raft 节点的快照安装请求
     */
    InstallSnapshotReply handleInstallSnapshot(InstallSnapshotArgs arg);
    /**
     * @brief 获取节点 id
     */
    int64_t getNodeId() const { return m_id;}
    /**
     * @brief 持久化，外部调用，有加锁
     * @param index 从该 idex 之前的日志都去掉
     * @param snap 快照数据
     */
    void persistStateAndSnapshot(int64_t index, const std::string& snap);
    /**
     * @brief 输出 Raft 节点状态
     */
    std::string toString();
    /**
     * @brief 获取心跳超时时间
     */
    static uint64_t GetStableHeartbeatTimeout();
    /**
     * @brief 获取随机选举时间
     */
    static uint64_t GetRandomizedElectionTimeout();
private:
    /**
     * @brief 转化为 Follower
     * @param[in] term 任期
     * @param[in] leaderId 任期领导人id，如果还未选举出来默认为-1
     */
    void becomeFollower(int64_t term, int64_t leaderId = -1);
    /**
     * @brief 转化为 Candidate
     */
    void becomeCandidate();
    /**
     * @brief 转换为 Leader
     */
    void becomeLeader();
    /**
     * @brief 重置选举定时器
     */
    void rescheduleElection();
    /**
     * @brief 重置心跳定时器
     */
    void resetHeartbeatTimer();
    /**
     * @brief 对一个节点发起复制请求
     * @param peer 对方节点 id
     */
    void replicateOneRound(int64_t peer);
    /**
     * @brief 用来往 applyCh 中 push 提交的日志
     */
    void applier();
    /**
     * @brief 开始选举，发起异步投票
     */
    void startElection();
    /**
     * @brief 广播心跳
     */
    void broadcastHeartbeat();
    /**
     * @brief 持久化，内部调用，不加锁
     */
    void persist(Snapshot::ptr snap = nullptr);
    /**
     * @brief 发起一条消息，不加锁
     */
    std::optional<Entry> Propose(const std::string& data);

private:
    MutexType m_mutex;
    // 节点状态，初始为 Follower
    RaftState m_state = Follower;
    // 该 raft 节点的唯一id
    int64_t m_id;
    // 任期内的 leader id，用于 follower 返回给客户端让客户端重定向请求到leader，-1表示无leader
    int64_t m_leaderId = -1;
    // 服务器已知最新的任期（在服务器首次启动时初始化为0，单调递增）
    int64_t m_currentTerm = 0;
    // 当前任期内收到选票的 candidateId，如果没有投给任何候选人 则为-1
    int64_t m_votedFor = -1;
    // 日志条目，每个条目包含了用于状态机的命令，以及领导人接收到该条目时的任期（初始索引为1）
    RaftLog m_logs;
    // 远端的 raft 节点，id 到节点的映射
    std::map<int64_t, RaftPeer::ptr> m_peers;
    // 对于每一台服务器，发送到该服务器的下一个日志条目的索引（初始值为领导人最后的日志条目的索引+1）
    std::map<int64_t, int64_t> m_nextIndex;
    // 对于每一台服务器，已知的已经复制到该服务器的最高日志条目的索引（初始值为0，单调递增）
    std::map<int64_t, int64_t> m_matchIndex;
    // 选举定时器，超时后节点将转换为候选人发起投票
    CycleTimerTocken m_electionTimer;
    // 心跳定时器，领导者定时发送日志维持心跳，和同步日志
    CycleTimerTocken m_heartbeatTimer;
    // 一次发送的最大日志数量 TODO
    int64_t m_maxLogSize = 1000;
    // 持久化
    Persister::ptr m_persister;
    co::co_condition_variable m_applyCond;
    // 用来已通过raft达成共识的已提交的提议通知给其它组件的信道。
    co::co_chan<ApplyMsg> m_applyChan;
};

}
#endif //ACID_RAFT_NODE_H
