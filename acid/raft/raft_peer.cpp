//
// Created by zavier on 2022/7/19.
//

#include "../common/config.h"
#include "../raft/raft_peer.h"

namespace acid::raft {
static auto g_logger = GetLogInstance();
// rpc 连接超时时间
static ConfigVar<uint64_t>::ptr g_rpc_timeout =
        Config::Lookup<uint64_t>("raft.rpc.timeout", 3000, "raft rpc timeout(ms)");
// rpc 连接重试次数
static ConfigVar<uint32_t>::ptr g_connect_retry =
        Config::Lookup<uint32_t>("raft.rpc.connect_retry", 3, "raft rpc connect retry times");

static uint64_t s_rpc_timeout;
static uint32_t s_connect_retry;

struct _RaftPeerIniter{
    _RaftPeerIniter(){
        s_rpc_timeout = g_rpc_timeout->getValue();
        g_rpc_timeout->addListener([](const uint64_t& old_val, const uint64_t& new_val){
            SPDLOG_LOGGER_INFO(g_logger, "raft rpc timeout changed from {} to {}", old_val, new_val);
            s_rpc_timeout = new_val;
        });
        s_connect_retry = g_connect_retry->getValue();
        g_rpc_timeout->addListener([](const uint32_t& old_val, const uint32_t& new_val){
            SPDLOG_LOGGER_INFO(g_logger, "raft rpc timeout changed from {} to {}", old_val, new_val);
            s_connect_retry = new_val;
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

bool RaftPeer::connect() {
    if (!m_client->isClose()) {
        return true;
    }
    for (int i = 1; i <= (int)s_connect_retry; ++i) {
        // 重连 Raft 节点
        m_client->connect(m_address);
        if (!m_client->isClose()) {
            return true;
        }
        co_sleep(10 * i);
    }
    return false;
}

std::optional<RequestVoteReply> RaftPeer::requestVote(const RequestVoteArgs& arg) {
    if (!connect()) {
        return std::nullopt;
    }
    rpc::Result<RequestVoteReply> result = m_client->call<RequestVoteReply>("RaftNode::handleRequestVote", arg);
    if (result.getCode() == rpc::RpcState::RPC_SUCCESS) {
        return result.getVal();
    }
    if (result.getCode() == rpc::RpcState::RPC_CLOSED) {
        m_client->close();
    }
    SPDLOG_LOGGER_DEBUG(g_logger, "Rpc call Node[{}] method [ RaftNode::handleRequestVote ] failed, code is {}, msg is {}, RequestVoteArgs is {}",
                      m_id, result.getCode(), result.getMsg(), arg.toString());
    return std::nullopt;
}

std::optional<AppendEntriesReply> RaftPeer::appendEntries(const AppendEntriesArgs& arg) {
    if (!connect()) {
        return std::nullopt;
    }
    rpc::Result<AppendEntriesReply> result = m_client->call<AppendEntriesReply>("RaftNode::handleAppendEntries", arg);
    if (result.getCode() == rpc::RpcState::RPC_SUCCESS) {
        return result.getVal();
    }
    if (result.getCode() == rpc::RpcState::RPC_CLOSED) {
        m_client->close();
    }
    SPDLOG_LOGGER_DEBUG(g_logger, "Rpc call Node[{}] method [ RaftNode::handleAppendEntries ] failed, code is {}, msg is {}",
                        m_id, result.getCode(), result.getMsg());
    return std::nullopt;
}

std::optional<InstallSnapshotReply> RaftPeer::installSnapshot(const InstallSnapshotArgs& arg) {
    if (!connect()) {
        return std::nullopt;
    }
    rpc::Result<InstallSnapshotReply> result = m_client->call<InstallSnapshotReply>("RaftNode::handleInstallSnapshot", arg);
    if (result.getCode() == rpc::RpcState::RPC_SUCCESS) {
        return result.getVal();
    }
    if (result.getCode() == rpc::RpcState::RPC_CLOSED) {
        m_client->close();
    }
    SPDLOG_LOGGER_DEBUG(g_logger, "Rpc call Node[{}] method [ RaftNode::handleInstallSnapshot ] failed, code is {}, msg is {}, InstallSnapshotArgs is {}",
                      m_id, result.getCode(), result.getMsg(), arg.toString());
    return std::nullopt;
}

}