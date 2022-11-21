//
// Created by zavier on 2022/7/19.
//

#ifndef ACID_RAFT_PEER_H
#define ACID_RAFT_PEER_H
#include <optional>
#include "../rpc/rpc_client.h"
#include "raft_log.h"
namespace acid::raft {
using namespace acid::rpc;

/**
 * @brief RequestVote rpc 调用的参数
 */
struct RequestVoteArgs {
    int64_t term;          // 候选人的任期
    int64_t candidateId;   // 请求选票的候选人的 ID
    int64_t lastLogIndex;  // 候选人的最后日志条目的索引值
    int64_t lastLogTerm;   // 候选人的最后日志条目的任期号
    std::string toString() const {
        std::string str = fmt::format("Term: {}, CandidateId: {}, LastLogIndex: {}, LastLogTerm: {}",
                                      term, candidateId, lastLogIndex, lastLogTerm);
        return "{" + str + "}";
    }
    friend rpc::Serializer& operator<<(rpc::Serializer& s, const RequestVoteArgs& arg) {
        s << arg.term << arg.candidateId << arg.lastLogIndex << arg.lastLogTerm;
        return s;
    }
    friend rpc::Serializer& operator>>(rpc::Serializer& s, RequestVoteArgs& arg) {
        s >> arg.term >> arg.candidateId >> arg.lastLogIndex >> arg.lastLogTerm;
        return s;
    }
};

/**
 * @brief RequestVote rpc 调用的返回值
 */
struct RequestVoteReply {
    int64_t term;       // 当前任期号大于候选人id时，候选人更新自己的任期号，并切换为追随者
    int64_t leaderId;   // 当前任期的leader
    bool voteGranted;   // true表示候选人赢得了此张选票
    std::string toString() const {
        std::string str = fmt::format("Term: {}, LeaderId: {}, VoteGranted: {}", term, leaderId, voteGranted);
        return "{" + str + "}";
    }
    friend rpc::Serializer& operator<<(rpc::Serializer& s, const RequestVoteReply& reply) {
        s << reply.term << reply.leaderId << reply.voteGranted;
        return s;
    }
    friend rpc::Serializer& operator>>(rpc::Serializer& s, RequestVoteReply& reply) {
        s >> reply.term >> reply.leaderId >> reply.voteGranted;
        return s;
    }
};

/**
 * @brief AppendEntries rpc 调用的参数
 */
struct AppendEntriesArgs {
    AppendEntriesArgs() = default;
    int64_t term;               // 领导人的任期
    int64_t leaderId;           // 领导id
    int64_t prevLogIndex;       // 紧邻新日志条目之前的那个日志条目的索引
    int64_t prevLogTerm;        // 紧邻新日志条目之前的那个日志条目的任期
    std::vector<Entry> entries; // 需要被保存的日志条目（被当做心跳使用时，则日志条目内容为空；为了提高效率可能一次性发送多个）
    int64_t leaderCommit;	    // 领导人的已知已提交的最高的日志条目的索引
    std::string toString() const {
        std::string str = "{ term = " + std::to_string(term) +
                          ", leaderId = " + std::to_string(leaderId) +
                          ", prevLogIndex = " + std::to_string(prevLogIndex) +
                          ", prevLogTerm = " + std::to_string(prevLogTerm) +
                          ", entries size = " + std::to_string(entries.size()) +
                          ", leaderCommit = " + std::to_string(leaderCommit) +
                          " }";
        return str;
    }
    friend rpc::Serializer& operator<<(rpc::Serializer& s, const AppendEntriesArgs& arg) {
        s << arg.term << arg.leaderId << arg.prevLogIndex << arg.prevLogTerm << arg.entries << arg.leaderCommit;
        return s;
    }
    friend rpc::Serializer& operator>>(rpc::Serializer& s, AppendEntriesArgs& arg) {
        s >> arg.term >> arg.leaderId >> arg.prevLogIndex >> arg.prevLogTerm >> arg.entries >> arg.leaderCommit;
        return s;
    }
};

/**
 * @brief AppendEntries rpc 调用的返回值
 */
struct AppendEntriesReply {
    bool success = false;          // 如果跟随者所含有的条目和 prevLogIndex 以及 prevLogTerm 匹配上了，则为 true
    int64_t term = 0;              // 当前任期，如果大于领导人的任期则切换为追随者
    int64_t leaderId = 0;          // 当前任期的leader
    int64_t conflictTerm = 0;
    int64_t conflictIndex = 0;
    std::string toString() const {
        std::string str = fmt::format("Success: {}, Term: {}, LeaderId: {}, ConflictTerm: {}, ConflictIndex: {}",
                                      success, term, leaderId, conflictTerm, conflictIndex);
        return "{" + str + "}";
    }
    friend rpc::Serializer& operator<<(rpc::Serializer& s, const AppendEntriesReply& reply) {
        s << reply.success << reply.term << reply.leaderId << reply.conflictTerm << reply.conflictIndex;
        return s;
    }
    friend rpc::Serializer& operator>>(rpc::Serializer& s, AppendEntriesReply& reply) {
        s >> reply.success >> reply.term >> reply.leaderId >> reply.conflictTerm >> reply.conflictIndex;
        return s;
    }
};

struct InstallSnapshotArgs {
    int64_t term;               // 领导人的任期号
    int64_t leaderId;           // 领导人的 ID，以便于跟随者重定向请求
    Snapshot snapshot;          // 快照
    std::string toString() const {
        std::string str = fmt::format("Term: {}, LeaderId: {}, Snapshot.Metadata.Index: {}, Snapshot.Metadata.Term: {}",
                                      term, leaderId, snapshot.metadata.index, snapshot.metadata.term);
        return "{" + str + "}";
    }
    friend rpc::Serializer& operator<<(rpc::Serializer& s, const InstallSnapshotArgs& arg) {
        s << arg.term << arg.leaderId << arg.snapshot;
        return s;
    }
    friend rpc::Serializer& operator>>(rpc::Serializer& s, InstallSnapshotArgs& arg) {
        s >> arg.term >> arg.leaderId >> arg.snapshot;
        return s;
    }
};

struct InstallSnapshotReply {
    int64_t term;           // 当前任期号，便于领导人更新自己
    int64_t leaderId;       // 当前任期领导人
    std::string toString() const {
        std::string str = fmt::format("Term: {}, LeaderId: {}", term, leaderId);
        return "{" + str + "}";
    }
    friend rpc::Serializer& operator<<(rpc::Serializer& s, const InstallSnapshotReply& reply) {
        s << reply.term << reply.leaderId;
        return s;
    }
    friend rpc::Serializer& operator>>(rpc::Serializer& s, InstallSnapshotReply& reply) {
        s >> reply.term >> reply.leaderId;
        return s;
    }
};

/**
 * @brief RaftNode 通过 RaftPeer 调用远端 Raft 节点，封装了 rpc 请求
 */
class RaftPeer {
public:
    using ptr = std::shared_ptr<RaftPeer>;
    RaftPeer(int64_t id, Address::ptr address);

    std::optional<RequestVoteReply> requestVote(const RequestVoteArgs& arg);

    std::optional<AppendEntriesReply> appendEntries(const AppendEntriesArgs& arg);

    std::optional<InstallSnapshotReply> installSnapshot(const InstallSnapshotArgs& arg);

    Address::ptr getAddress() const { return m_address;}
private:
    int64_t m_id;
    rpc::RpcClient::ptr m_client;
    Address::ptr m_address;
};

}
#endif //ACID_RAFT_PEER_H
