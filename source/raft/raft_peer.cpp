//
// Created by zavier on 2022/7/19.
//

#include "acid/config.h"
#include "acid/raft/raft_peer.h"

namespace acid::raft {
static acid::Logger::ptr g_logger = ACID_LOG_NAME("raft");
static ConfigVar<uint64_t>::ptr g_timer_election_top =
        Config::Lookup<size_t>("raft.timer.election.top",3000,"raft election timeout(ms) top");

static uint64_t s_timer_election_top_ms;

struct _RaftPeerIniter{
    _RaftPeerIniter(){
        s_timer_election_top_ms = g_timer_election_top->getValue();
        g_timer_election_top->addListener([](const uint64_t& old_val, const uint64_t& new_val){
            ACID_LOG_INFO(g_logger) << "raft election timeout top changed from "
                                    << old_val << " to " << new_val;
            s_timer_election_top_ms = new_val;
        });
    }
};

// 初始化配置
static _RaftPeerIniter s_initer;

RaftPeer::RaftPeer(int64_t id, Address::ptr address)
        : m_id(id), m_address(address) {
    // 关闭 RpcClient 自带的心跳
    m_client = std::make_shared<rpc::RpcClient>(false);
    // 设置rpc超时时间
    m_client->setTimeout(s_timer_election_top_ms);
}

std::optional<RequestVoteReply> RaftPeer::requestVote(const RequestVoteArgs& arg) {
    if (m_client->isClose()) {
        // 重连 Raft 节点
        m_client->connect(m_address);
    }
    rpc::Result<RequestVoteReply> result = m_client->call<RequestVoteReply>("RaftNode::handleRequestVote", arg);
    if (result.getCode() == rpc::RpcState::RPC_SUCCESS) {
        return result.getVal();
    }
    ACID_LOG_FMT_INFO(g_logger,
                      "Rpc call Node[%ld] method [ RaftNode::handleRequestVote ] failed, "
                      "code is %ld, msg is %s, RequestVoteArgs is %s",
                      m_id, result.getCode(), result.getMsg().c_str(), arg.toString().c_str());
    return std::nullopt;
}

std::optional<AppendEntriesReply> RaftPeer::appendEntries(const AppendEntriesArgs& arg) {
    if (m_client->isClose()) {
        // 重连 Raft 节点
        m_client->connect(m_address);
    }
    rpc::Result<AppendEntriesReply> result = m_client->call<AppendEntriesReply>("RaftNode::handleAppendEntries", arg);
    if (result.getCode() == rpc::RpcState::RPC_SUCCESS) {
        return result.getVal();
    }
//    ACID_LOG_DEBUG(g_logger) << "call rpc [ RaftNode::handleAppendEntries ] fail, code="
//                             << result.getCode() << ", msg=" << result.getMsg();
    return std::nullopt;
}

std::optional<InstallSnapshotReply> RaftPeer::installSnapshot(const InstallSnapshotArgs& arg) {
    if (m_client->isClose()) {
        // 重连 Raft 节点
        m_client->connect(m_address);
    }
    rpc::Result<InstallSnapshotReply> result = m_client->call<InstallSnapshotReply>("RaftNode::handleInstallSnapshot", arg);
    if (result.getCode() == rpc::RpcState::RPC_SUCCESS) {
        return result.getVal();
    }
    ACID_LOG_FMT_INFO(g_logger,
                      "Rpc call Node[%ld] method [ RaftNode::handleInstallSnapshot ] failed, "
                      "code is %ld, msg is %s, InstallSnapshotArgs is %s",
                      m_id, result.getCode(), result.getMsg().c_str(), arg.toString().c_str());
    return std::nullopt;
}

}