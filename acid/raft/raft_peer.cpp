//
// Created by zavier on 2022/7/19.
//

#include "acid/common/config.h"
#include "acid/raft/raft_peer.h"

namespace acid::raft {
static auto g_logger = GetLogInstance();
static ConfigVar<uint64_t>::ptr g_rpc_timeout =
        Config::Lookup<size_t>("raft.rpc.timeout",3000,"raft rpc timeout(ms)");

static uint64_t s_rpc_timeout;

struct _RaftPeerIniter{
    _RaftPeerIniter(){
        s_rpc_timeout = g_rpc_timeout->getValue();
        g_rpc_timeout->addListener([](const uint64_t& old_val, const uint64_t& new_val){
            SPDLOG_LOGGER_INFO(g_logger, "raft rpc timeout changed from {} to {}", old_val, new_val);
            s_rpc_timeout = new_val;
        });
    }
};

// 初始化配置
static _RaftPeerIniter s_initer;

RaftPeer::RaftPeer(int64_t id, Address::ptr address)
        : m_id(id), m_address(address) {
    // 关闭 RpcClient 自带的心跳
    m_client = std::make_shared<rpc::RpcClient>();
    // 关闭心跳
    m_client->setHeartbeat(false);
    // 设置rpc超时时间
    m_client->setTimeout(s_rpc_timeout);
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
    SPDLOG_LOGGER_DEBUG(g_logger, "Rpc call Node[{}] method [ RaftNode::handleRequestVote ] failed, code is {}, msg is {}, RequestVoteArgs is {}",
                      m_id, result.getCode(), result.getMsg(), arg.toString());
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
    SPDLOG_LOGGER_DEBUG(g_logger, "Rpc call Node[{}] method [ RaftNode::handleAppendEntries ] failed, code is {}, msg is {}",
                        m_id, result.getCode(), result.getMsg());
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
    SPDLOG_LOGGER_DEBUG(g_logger, "Rpc call Node[{}] method [ RaftNode::handleInstallSnapshot ] failed, code is {}, msg is {}, InstallSnapshotArgs is {}",
                      m_id, result.getCode(), result.getMsg(), arg.toString());
    return std::nullopt;
}

}