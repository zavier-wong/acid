//
// Created by zavier on 2022/6/20.
//

#include <random>
#include "acid/log.h"
#include "acid/raft/raft_node.h"
#include "acid/config.h"

namespace acid::raft {
static acid::Logger::ptr g_logger = ACID_LOG_NAME("raft");

static ConfigVar<uint64_t>::ptr g_timer_election_base =
        Config::Lookup<size_t>("raft.timer.election.base",1500,"raft election timeout(ms) base");
static ConfigVar<uint64_t>::ptr g_timer_election_top =
        Config::Lookup<size_t>("raft.timer.election.top",3000,"raft election timeout(ms) top");
static ConfigVar<uint64_t>::ptr g_timer_heartbeat =
        Config::Lookup<size_t>("raft.timer.heartbeat",300,"raft heartbeat timeout(ms)");

// 选举超时时间，从base~top的区间里随机选择
static uint64_t s_timer_election_base_ms;
static uint64_t s_timer_election_top_ms;
// 心跳超时时间
// 注意，心跳超时必须小于选举超时时间
static uint64_t s_timer_heartbeat_ms;

struct _RaftNodeIniter{
    _RaftNodeIniter(){
        s_timer_election_base_ms = g_timer_election_base->getValue();
        g_timer_election_base->addListener([](const uint64_t& old_val, const uint64_t& new_val){
            ACID_LOG_INFO(g_logger) << "raft election timeout base changed from "
                                    << old_val << " to " << new_val;
            s_timer_election_base_ms = new_val;
        });
        s_timer_election_top_ms = g_timer_election_top->getValue();
        g_timer_election_top->addListener([](const uint64_t& old_val, const uint64_t& new_val){
            ACID_LOG_INFO(g_logger) << "raft election timeout top changed from "
                                    << old_val << " to " << new_val;
            s_timer_election_top_ms = new_val;
        });
        s_timer_heartbeat_ms = g_timer_heartbeat->getValue();
        g_timer_heartbeat->addListener([](const uint64_t& old_val, const uint64_t& new_val){
            ACID_LOG_INFO(g_logger) << "raft heartbeat timeout changed from "
                                    << old_val << " to " << new_val;
            s_timer_heartbeat_ms = new_val;
        });
    }
};

// 初始化配置
[[maybe_unused]]
static _RaftNodeIniter s_initer;


RaftNode::RaftNode(int64_t id, RaftLog rl, Channel<Entry> applyChan, Channel<std::string> proposeChan)
        : m_id(id)
        , m_logs(rl)
        , m_applyChan(applyChan)
        , m_proposeChan(proposeChan){
    rpc::RpcServer::setName("Raft-Node[" + std::to_string(id) + "]");
    // 注册服务 RequestVote
    registerMethod("RaftNode::handleRequestVote",[this](RequestVoteArgs args){
        return handleRequestVote(args);
    });
    // 注册服务 AppendEntries
    registerMethod("RaftNode::handleAppendEntries", [this](AppendEntriesArgs args){
        return handleAppendEntries(std::move(args));
    });
    // 注册服务 InstallSnapshot
    registerMethod("RaftNode::handleInstallSnapshot", [this](InstallSnapshotArgs args){
        return handleInstallSnapshot(std::move(args));
    });

}

RaftNode::~RaftNode() {
    MutexType::Lock lock(m_mutex);
    m_proposeChan.close();
    m_applyChan.close();
    if (m_heartbeatTimer) {
        m_heartbeatTimer->cancel();
        m_heartbeatTimer = nullptr;
    }
    if (m_electionTimer) {
        m_electionTimer->cancel();
        m_electionTimer = nullptr;
    }
}

bool RaftNode::start() {
    bool ret = RpcServer::start();
    MutexType::Lock lock(m_mutex);
    becomeFollower(0);

    m_electionTimer = IOManager::GetThis()->addTimer(-1, [this]{
        MutexType::Lock lock(m_mutex);
        if (m_state != RaftState::Leader) {
            becomeCandidate();
            // 异步投票，不阻塞选举定时器
            startElection();
        }
        rescheduleElection();
    });

    m_heartbeatTimer = IOManager::GetThis()->addTimer(GetStableHeartbeatTimeout(), [this]{
        MutexType::Lock lock(m_mutex);
        if (m_state == RaftState::Leader) {
            // broadcast
            broadcastHeartbeat(true);
        }
    }, true);

    m_heartbeatTimer->cancel();

    go [this]{
        applier();
    };

    go[this] {
        // "%x not forwarding to leader %x at term %ld; dropping proposal"
        std::string data;
        while (m_proposeChan >> data) {
            Entry ent;
            ent.data = data;
            MutexType::Lock lock(m_mutex);
            if (m_state != RaftState::Leader) {
                ACID_LOG_FMT_DEBUG(g_logger,
                   "Node[%ld] no leader at term %ld; dropping proposal",
                   m_id, m_currentTerm);
            }
            appendEntry(ent);
            ACID_LOG_FMT_DEBUG(g_logger,
               "Node[%ld] receives a new log entry[%s] to replicate in term %ld",
               m_id, data.c_str(), m_currentTerm);
            broadcastHeartbeat(false);
        }
    };

    return ret;
}

void RaftNode::startElection() {
    RequestVoteArgs request{};
    request.term = m_currentTerm;
    request.candidateId = m_id;
    request.lastLogIndex = m_logs.lastIndex();
    request.lastLogTerm = m_logs.lastTerm();

    ACID_LOG_FMT_DEBUG(g_logger,
       "Node[%ld] starts election with RequestVoteArgs %s",
       m_id, request.toString().c_str());
    m_votedFor = m_id;
    // 统计投票结果，自己开始有一票
    std::shared_ptr<int64_t> grantedVotes = std::make_shared<int64_t>(1);

    for (auto& peer: m_peers) {
        // 使用协程发起异步投票，不阻塞选举定时器，才能在选举超时后发起新的选举
        go [grantedVotes, request, peer, this] {
            auto reply = peer.second->requestVote(request);
            if (!reply)
                return;
            MutexType::Lock lock(m_mutex);
            ACID_LOG_FMT_DEBUG(g_logger,
               "Node[%ld] receives RequestVoteReply %s from Node[%ld] "
               "after sending RequestVoteArgs %s in term %ld",
               m_id, reply->toString().c_str(),
               peer.first, request.toString().c_str(),
               m_currentTerm);
            // 检查自己状态是否改变
            if (m_currentTerm == request.term && m_state == RaftState::Candidate) {
                if (reply->voteGranted) {
                    ++(*grantedVotes);
                    // 赢得选举
                    if (*grantedVotes > ((int64_t)m_peers.size() + 1) / 2) {
                        ACID_LOG_FMT_DEBUG(g_logger,
                           "Node[%ld] receives majority votes in term %ld",
                           m_id, m_currentTerm);
                        becomeLeader();
                    }
                } else if (reply->term > m_currentTerm) {
                    ACID_LOG_FMT_DEBUG(g_logger,
                       "Node[%ld] finds a new leader Node[%ld] with term %ld and steps down in term %ld",
                       m_id, peer.first, reply->term, m_currentTerm);
                    becomeFollower(reply->term, reply->leaderId);
                    rescheduleElection();
                }
            }
        };
    }
}

void RaftNode::applier() {
    while (!isStop()) {
        MutexType::Lock lock(m_mutex);
        // 如果没有需要apply的日志则等待
        while (m_logs.getApplied() >= m_logs.getCommitted()) {
            m_applyCond.wait(lock);
        }

        auto last_commit = m_logs.getCommitted();
        auto ents = m_logs.nextEntries();

        lock.unlock();
        for (auto& entry: ents) {
            m_applyChan << entry;
        }
        lock.lock();
        ACID_LOG_FMT_DEBUG(g_logger,
           "Node[%ld] applies entries %ld-%ld in term %ld",
           m_id, m_logs.getApplied(), last_commit, m_currentTerm);
        // 1. push applyCh 结束之后更新 lastApplied 的时候一定得用之前的 commitIndex ，因为很可能在 push channel 期间发生了改变。
        // 2. 防止与 installSnapshot 并发导致 lastApplied 回退：需要注意到，applier 协程在 push channel 时，中间可能夹杂有 snapshot 也在 push channel。
        // 如果该 snapshot 有效，那么在 CondInstallSnapshot 函数里上层状态机和 raft 模块就会原子性的发生替换，即上层状态机更新为 snapshot 的状态，
        // raft 模块更新 log, commitIndex, lastApplied 等等，此时如果这个 snapshot 之后还有一批旧的 entry 在 push channel，
        // 那上层服务需要能够知道这些 entry 已经过时，不能再 apply，同时 applier 这里也应该加一个 Max 自身的函数来防止 lastApplied 出现回退。
        // TODO
        m_logs.appliedTo(std::max(m_logs.getApplied(), last_commit));
    }
}

void RaftNode::replicator(int64_t peer) {
    MutexType::Lock locK(m_mutex);
    while (!isStop()) {
        while (!needReplicating(peer)) {
            m_replicatorCond[peer].wait(locK);
        }
        replicateOneRound(peer);
    }
}

void RaftNode::broadcastHeartbeat(bool isHeartbeat) {
    for ([[maybe_unused]] auto [id, _]: m_peers) {
        if (isHeartbeat) {
            go [id = id, this]{
                replicateOneRound(id);
            };
        } else {
            m_replicatorCond[id].notify();
        }
    }
}

void RaftNode::replicateOneRound(int64_t peer) {
    // 发送 rpc的时候一定不要持锁，否则很可能产生死锁。
    MutexType::Lock lock(m_mutex);
    if (m_state != RaftState::Leader) {
        return;
    }
    int64_t term = m_logs.term(m_nextIndex[peer] - 1);

    auto [ents, err] = m_logs.entries(m_nextIndex[peer], m_maxLogSize);

    // 如果未能获得term或entries，则发送快照
    if (term < 0 || err) {
        InstallSnapshotArgs request{};
        Snapshot::ptr snapshot = m_logs.snapshot();
        if (!snapshot || snapshot->empty()) {
            ACID_LOG_FATAL(g_logger) << "need non-empty snapshot";
        }
        request.snapshot = *snapshot;
        request.term = m_currentTerm;
        request.leaderId = m_id;

        ACID_LOG_FMT_DEBUG(g_logger,
           "Node[%ld] [firstIndex: %ld, commit: %ld] sent snapshot[index: %ld, term: %ld] to Node[%ld]",
           m_id, m_logs.firstIndex(), m_logs.getCommitted(), snapshot->metadata.index,
           snapshot->metadata.index, peer);

        lock.unlock();

        auto reply = m_peers[peer]->installSnapshot(request);
        if (!reply)
            return;

        lock.lock();

        ACID_LOG_FMT_DEBUG(g_logger,
           "Node[%ld] receives InstallSnapshotReply %s from Node[%ld] "
           "after sending InstallSnapshotArgs %s in term %ld",
           m_id, reply->toString().c_str(),
           peer, request.toString().c_str(),
           m_currentTerm);

        // 如果因为网络原因集群选举出新的leader则自己变成follower
        if (reply->term > m_currentTerm) {
            becomeFollower(reply->term, reply->leaderId);
        }
    } else {
        AppendEntriesArgs request{};
        request.term = m_currentTerm;
        request.leaderId = m_id;
        request.leaderCommit = m_logs.getCommitted();
        request.prevLogIndex = m_nextIndex[peer] - 1;
        request.prevLogTerm = term;
        request.entries = ents;

        lock.unlock();

        auto reply = m_peers[peer]->appendEntries(request);

        if (!reply) {
            return;
        }

        lock.lock();

        ACID_LOG_FMT_DEBUG(g_logger,
           "Node[%ld] receives AppendEntriesReply %s from Node[%ld] "
           "after sending AppendEntriesArgs %s in term %ld",
           m_id, reply->toString().c_str(),
           peer, request.toString().c_str(),
           m_currentTerm);

        if (m_state != RaftState::Leader) {
            return;
        }

        // 如果因为网络原因集群选举出新的leader则自己变成follower
        if (reply->term > m_currentTerm) {
            becomeFollower(reply->term, reply->leaderId);
            return;
        }
        // 过期的响应
        if (reply->term < m_currentTerm) {
            return;
        }
        // 日志追加失败，根据 conflictIndex 和 conflictTerm 更新 m_nextIndex 和 m_matchIndex
        if (!reply->success) {
            m_nextIndex[peer] = reply->conflictIndex;
            m_matchIndex[peer] = std::min(m_matchIndex[peer], reply->conflictIndex - 1);
            return;
        }
        // 只是一个空的心跳包就直接返回
        if (ents.empty()) {
            return;
        }

        // 日志追加成功，则更新 m_nextIndex 和 m_matchIndex
        m_nextIndex[peer] += (int64_t)ents.size();
        m_matchIndex[peer] = m_nextIndex[peer] - 1;

        int64_t last_index = m_matchIndex[peer];

        if (m_logs.getCommitted() >= last_index
                // 只有领导人当前任期里的日志条目可以被提交
                || ents.back().term != m_currentTerm) {
            return;
        }

        // 计算副本数目大于节点数量的一半才提交一个当前任期内的日志
        int64_t vote = 1;
        for ([[maybe_unused]] auto [_, match]: m_matchIndex) {
            if (match >= last_index) {
                ++vote;
            }
        }
        // 假设存在 N 满足N > commitIndex，使得大多数的 matchIndex[i] ≥ N以及log[N].term == currentTerm 成立，
        // 则令 commitIndex = N（5.3 和 5.4 节）
        if (vote > ((int64_t)m_peers.size() + 1) / 2) {
            m_logs.maybeCommit(last_index, m_currentTerm);
        }
    }

}

bool RaftNode::needReplicating(int64_t peer) {
    MutexType::Lock lock(m_mutex);
    return m_state == RaftState::Leader && m_matchIndex[peer] < m_logs.lastIndex();
}


RequestVoteReply RaftNode::handleRequestVote(RequestVoteArgs request) {
    MutexType::Lock lock(m_mutex);
    RequestVoteReply reply{};
    Defer {
          // 投票后节点的状态
          ACID_LOG_FMT_DEBUG(g_logger,
             "Node[%ld] before processing RequestVoteArgs %s "
             "and reply RequestVoteReply %s, "
             "state is %s",
             m_id, request.toString().c_str(), reply.toString().c_str(), toString().c_str());
    };

    // 拒绝给任期小于自己的候选人投票
    if (request.term < m_currentTerm ||
            // 多个候选人发起选举的情况
            (request.term == m_currentTerm && m_votedFor != -1 && m_votedFor != request.candidateId)) {
        reply.term = m_currentTerm;
        reply.leaderId = m_leaderId;
        reply.voteGranted = false;
        return reply;
    }

    // 任期比自己大，变成追随者
    if (request.term > m_currentTerm) {
        becomeFollower(request.term);
    }

    // 拒绝掉那些日志没有自己新的投票请求
    if (!m_logs.isUpToDate(request.lastLogIndex, request.lastLogTerm)) {
        reply.term = m_currentTerm;
        reply.leaderId = m_leaderId;
        reply.voteGranted = false;
        return reply;
    }

    // 投票给候选人
    m_votedFor = request.candidateId;

    // 重点，成功投票后才重置选举定时器，这样有助于网络不稳定条件下选主的 liveness 问题
    // 例如：极端情况下，Candidate只能发送消息而收不到Follower的消息，Candidate会不断发起新
    // 一轮的选举，如果节点在becomeFollower后马上重置选举定时器，会导致节点无法发起选举，该集群
    // 无法选出新的 Leader。
    // 只是重置定时器的时机不一样，就导致了集群无法容忍仅仅 1 个节点故障
    rescheduleElection();

    // 返回给候选人的结果
    reply.term = m_currentTerm;
    reply.leaderId = m_leaderId;
    reply.voteGranted = true;

    return reply;
}

AppendEntriesReply RaftNode::handleAppendEntries(AppendEntriesArgs request) {
    MutexType::Lock lock(m_mutex);
    AppendEntriesReply reply{};
    // 由于心跳比较频繁，禁掉了这个log，只要将Defer_改为Defer就可以开启
    Defer_ {
        ACID_LOG_FMT_DEBUG(g_logger,
         "Node[%ld] before processing AppendEntriesArgs %s "
         "and reply AppendEntriesResponse %s, "
         "state is %s",
         m_id, request.toString().c_str(), reply.toString().c_str(), toString().c_str());
    };
    // 拒绝任期小于自己的 leader 的日志复制请求
    if (request.term < m_currentTerm) {
        reply.term = m_currentTerm;
        reply.leaderId = m_leaderId;
        reply.success = false;
        return reply;
    }

    // 对方任期大于自己或者自己为同一任期内败选的候选人则转变为 follower
    // 已经是该任期内的follower就不用变
    if (request.term > m_currentTerm ||
                (request.term == m_currentTerm && m_state == RaftState::Candidate)) {
        becomeFollower(request.term, request.leaderId);
    }
    // 自己为同一任期内的follower，更新选举定时器就行
    rescheduleElection();

    // 拒绝错误的日志追加请求
    if (request.prevLogIndex < m_logs.firstIndex()) {
        reply.term = 0;
        reply.leaderId = -1;
        reply.success = false;
        ACID_LOG_FMT_DEBUG(g_logger,
           "Node[%ld] receives unexpected AppendEntriesArgs %s from Node[%ld] "
           "because prevLogIndex %ld < firstLogIndex %ld",
           m_id, request.toString().c_str(), request.leaderId,
           request.prevLogIndex, m_logs.firstIndex());
        return reply;
    }

    int64_t last_index = m_logs.maybeAppend(request.prevLogIndex, request.prevLogTerm,
                       request.leaderCommit, request.entries);

    // 加速解决节点间日志冲突的优化
    if (last_index < 0) {
        reply.term = m_currentTerm;
        reply.leaderId = m_leaderId;
        reply.success = false;

        int64_t lastIndex = m_logs.lastIndex();
        if (lastIndex < request.prevLogIndex) {
            reply.conflictTerm = -1;
            reply.conflictIndex = lastIndex + 1;
        } else {
            // 找出当前冲突任期的第一条日志索引
            int64_t firstIndex = m_logs.firstIndex();
            reply.conflictTerm = m_logs.term(request.prevLogIndex - firstIndex);
            int64_t index = request.prevLogIndex -1;
            while (index >= firstIndex &&
                   (m_logs.term(index - firstIndex) == reply.conflictTerm)) {
                --index;
            }
            reply.conflictIndex = index;
        }
        return reply;
    }

    reply.term = m_currentTerm;
    reply.leaderId = m_leaderId;
    reply.success = true;
    return reply;
}

InstallSnapshotReply RaftNode::handleInstallSnapshot(InstallSnapshotArgs request) {
    MutexType::Lock lock(m_mutex);
    InstallSnapshotReply reply{};
    Defer {
          ACID_LOG_FMT_DEBUG(g_logger,
             "Node[%ld] before processing InstallSnapshotArgs %s "
             "and reply InstallSnapshotReply %s, "
             "state is %s",
             m_id, request.toString().c_str(), reply.toString().c_str(), toString().c_str());
    };
    reply.term = m_currentTerm;
    reply.leaderId = m_leaderId;

    if (request.term < m_currentTerm) {
        return reply;
    }

    if (request.term > m_currentTerm) {
        becomeFollower(request.term, request.leaderId);
    }
    rescheduleElection();

    const int64_t snap_index = request.snapshot.metadata.index;
    const int64_t snap_term = request.snapshot.metadata.term;

    // 过时的快照
    if (snap_index <= m_logs.getCommitted()) {
        ACID_LOG_FMT_DEBUG(g_logger,
           "Node[%ld] ignored snapshot [index: %ld, term: %ld]",
           m_id, snap_index, snap_term);
        return reply;
    }

    // 快速提交到快照的index
    if (m_logs.matchLog(snap_index, snap_term)) {
        ACID_LOG_FMT_DEBUG(g_logger,
           "Node[%ld] fast-forwarded commit to snapshot [index: %ld, term: %ld]",
           m_id, snap_index, snap_term);
        m_logs.commitTo(snap_index);
        return reply;
    }

    ACID_LOG_FMT_DEBUG(g_logger,
       "Node[%ld] starts to restore snapshot [index: %ld, term: %ld]",
       m_id, snap_index, snap_term);

    m_logs.restore(std::make_shared<Snapshot>(request.snapshot));

    return reply;
}

void RaftNode::addPeer(int64_t id, Address::ptr address) {
    MutexType::Lock lock(m_mutex);
    RaftPeer::ptr peer = std::make_shared<RaftPeer>(id, address);
    m_peers[id] = peer;
    m_nextIndex[id] = 0;
    m_matchIndex[id] = 0;
    // 对每个raft节点开启一个日志复制协程
    go [id, this] {
        replicator(id);
    };
    ACID_LOG_FMT_INFO(g_logger,
      "Node[%ld] add peer[%ld], address is %s",
      m_id, id, address->toString().c_str());
}

std::string RaftNode::toString() {
    std::string state;
    switch (m_state) {
        case Follower:
            state = "Follower";
            break;
        case Candidate:
            state = "Candidate";
            break;
        case Leader:
            state = "Leader";
            break;
    }

    std::string str = "{ id = " + std::to_string(m_id) +
                      ", state = " + state +
                      ", leaderId = " + std::to_string(m_leaderId) +
                      ", currentTerm = " + std::to_string(m_currentTerm) +
                      ", votedFor = " + std::to_string(m_votedFor) +
                      ", commitIndex = " + std::to_string(m_logs.getCommitted()) +
                      ", lastApplied = " + std::to_string(m_logs.getApplied()) +
                      " }";
    return str;
}

void RaftNode::becomeFollower(int64_t term, int64_t leaderId) {
    // 成为follower停止心跳定时器
    if (m_heartbeatTimer && m_state == RaftState::Leader) {
        m_heartbeatTimer->cancel();
    }

    m_state = RaftState::Follower;
    m_currentTerm = term;
    m_votedFor = -1;
    m_leaderId = leaderId;
    ACID_LOG_FMT_INFO(g_logger,
      "Node[%ld] became follower at term [%ld], state is %s",
      m_id, term, toString().c_str());
}

void RaftNode::becomeCandidate() {
    m_state = RaftState::Candidate;
    ++m_currentTerm;
    m_votedFor = m_id;
    m_leaderId = -1;
    ACID_LOG_FMT_INFO(g_logger,
      "Node[%ld] became Candidate at term %ld, state is %s",
      m_id, m_currentTerm, toString().c_str());
}

void RaftNode::becomeLeader() {
    // 成为leader停止选举定时器
    if (m_electionTimer) {
        m_electionTimer->cancel();
    }
    m_state = RaftState::Leader;
    m_leaderId = m_id;
    // 成为领导者后，领导者并不知道其它节点的日志情况，因此与其它节点需要同步那么日志，领导者并不知道集群其他节点状态，
    // 因此他选择了不断尝试。nextIndex、matchIndex 分别用来保存其他节点的下一个待同步日志index、已匹配的日志index。
    // nextIndex初始化值为lastIndex+1，即领导者最后一个日志序号+1，因此其实这个日志序号是不存在的，显然领导者也不
    // 指望一次能够同步成功，而是拿出一个值来试探。matchIndex初始化值为0，这个很好理解，因为他还未与任何节点同步成功过，
    // 所以直接为0。
    for ([[maybe_unused]] auto& [id, _] : m_peers) {
        m_nextIndex[id] = m_logs.lastIndex() + 1;
        m_matchIndex[id] = 0;
    }
    ACID_LOG_FMT_INFO(g_logger,
      "Node[%ld] became Leader at term %ld, state is %s",
      m_id, m_currentTerm, toString().c_str());

    // note: 即使日志已经被同步到了大多数个节点上，依然不能认为是已经提交了
    // 所以 leader 上任后应该提交一条空日志来提交之前的日志
    Entry empty;
    appendEntry(empty);
    // 成为领导者，发起一轮心跳
    broadcastHeartbeat(true);
    // 开启心跳定时器
    m_heartbeatTimer->reset(s_timer_heartbeat_ms, true);
}

void RaftNode::rescheduleElection() {
    if (m_electionTimer) {
        m_electionTimer->reset(GetRandomizedElectionTimeout(), true);
    }
}

uint64_t RaftNode::GetStableHeartbeatTimeout() {
    return s_timer_heartbeat_ms;
}

uint64_t RaftNode::GetRandomizedElectionTimeout() {
    static std::default_random_engine engine(acid::GetCurrentMS());
    static std::uniform_int_distribution<uint64_t> dist(s_timer_election_base_ms, s_timer_election_top_ms);
    return dist(engine);
}



bool RaftNode::appendEntry(Entry &entry) {
    auto ents = std::vector<Entry>(1, entry);
    bool rt = appendEntry(ents);
    entry = ents[0];
    return rt;
}

bool RaftNode::appendEntry(std::vector<Entry> &entries) {
    auto last_index = m_logs.lastIndex();
    for (int i = 0; i < (int64_t)entries.size(); ++i) {
        entries[i].term = m_currentTerm;
        entries[i].index = last_index + 1 + i;
    }
    m_logs.append(entries);
    return true;
}


}