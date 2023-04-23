//
// Created by zavier on 2022/6/20.
//

#include <random>
#include <utility>
#include "acid/raft/raft_node.h"
#include "acid/common/config.h"

namespace acid::raft {
static auto g_logger = GetLogInstance();

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
        g_timer_election_base->addListener([](const uint64_t& old_val, const uint64_t& new_val) {
            SPDLOG_LOGGER_INFO(g_logger, "raft election timeout base changed from {} to {}", old_val, new_val);
            s_timer_election_base_ms = new_val;
        });
        s_timer_election_top_ms = g_timer_election_top->getValue();
        g_timer_election_top->addListener([](const uint64_t& old_val, const uint64_t& new_val) {
            SPDLOG_LOGGER_INFO(g_logger, "raft election timeout top changed from {} to {}", old_val, new_val);
            s_timer_election_top_ms = new_val;
        });
        s_timer_heartbeat_ms = g_timer_heartbeat->getValue();
        g_timer_heartbeat->addListener([](const uint64_t& old_val, const uint64_t& new_val) {
            SPDLOG_LOGGER_INFO(g_logger, "raft heartbeat timeout changed from {} to {}", old_val, new_val);
            s_timer_heartbeat_ms = new_val;
        });
    }
};

// 初始化配置
[[maybe_unused]]
static _RaftNodeIniter s_initer;

RaftNode::RaftNode(std::map<int64_t, std::string>& servers, int64_t id, Persister::ptr persister, co::co_chan<ApplyMsg> applyChan)
        : m_id(id)
        , m_logs(persister, 1000)
        , m_persister(persister)
        , m_applyChan(std::move(applyChan)) {
    rpc::RpcServer::setName("Raft-Node[" + std::to_string(id) + "]");
    // 注册服务 RequestVote
    registerMethod(REQUEST_VOTE,[this](RequestVoteArgs args) {
        return handleRequestVote(std::move(args));
    });
    // 注册服务 AppendEntries
    registerMethod(APPEND_ENTRIES, [this](AppendEntriesArgs args) {
        return handleAppendEntries(std::move(args));
    });
    // 注册服务 InstallSnapshot
    registerMethod(INSTALL_SNAPSHOT, [this](InstallSnapshotArgs args) {
        return handleInstallSnapshot(std::move(args));
    });
    for (auto peer: servers) {
        if (peer.first == id)
            continue;
        Address::ptr address = Address::LookupAny(peer.second);
        // 添加节点
        addPeer(peer.first, address);
    }
}

RaftNode::~RaftNode() {
    stop();
}

void RaftNode::stop() {
    std::unique_lock<MutexType> lock(m_mutex);
    if (isStop()) {
        return;
    }
    m_applyChan.close();
    m_heartbeatTimer.stop();
    m_electionTimer.stop();
}

void RaftNode::start() {
    auto hs = m_persister->loadHardState();
    if (hs) {
        // 恢复崩溃前的状态
        m_currentTerm = hs->term;
        m_votedFor = hs->vote;
        SPDLOG_LOGGER_INFO(g_logger, "initialize from state persisted before a crash, term {}, vote {}, commit {}",
                           hs->term, hs->vote, hs->commit);
    } else {
        becomeFollower(0);
    }

    rescheduleElection();

    go [this] {
        applier();
    };

    RpcServer::start();
}

void RaftNode::startElection() {
    RequestVoteArgs request{};
    request.term = m_currentTerm;
    request.candidateId = m_id;
    request.lastLogIndex = m_logs.lastIndex();
    request.lastLogTerm = m_logs.lastTerm();

    SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] starts election with RequestVoteArgs {}", m_id, request.toString());
    m_votedFor = m_id;
    persist();
    // 统计投票结果，自己开始有一票
    std::shared_ptr<int64_t> grantedVotes = std::make_shared<int64_t>(1);

    for (auto& peer: m_peers) {
        // 使用协程发起异步投票，不阻塞选举定时器，才能在选举超时后发起新的选举
        go [grantedVotes, request, peer, this] {
            auto reply = peer.second->requestVote(request);
            if (!reply)
                return;
            std::unique_lock<MutexType> lock(m_mutex);
            SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] receives RequestVoteReply {} from Node[{}] after sending RequestVoteArgs {} in term {}",
                m_id, reply->toString(), peer.first, request.toString(), m_currentTerm);
            // 检查自己状态是否改变
            if (m_currentTerm == request.term && m_state == RaftState::Candidate) {
                if (reply->voteGranted) {
                    ++(*grantedVotes);
                    // 赢得选举
                    if (*grantedVotes > ((int64_t)m_peers.size() + 1) / 2) {
                        SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] receives majority votes in term {}", m_id, m_currentTerm);
                        becomeLeader();
                    }
                } else if (reply->term > m_currentTerm) {
                    SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] finds a new leader Node[{}] with term {} and steps down in term {}",
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
        std::unique_lock<MutexType> lock(m_mutex);
        // 如果没有需要apply的日志则等待
        while (!m_logs.hasNextEntries()) {
            m_applyCond.wait(lock);
        }

        auto last_commit = m_logs.committed();
        auto ents = m_logs.nextEntries();
        std::vector<ApplyMsg> msg;
        for (auto& ent: ents) {
            msg.emplace_back(ent);
        }

        lock.unlock();
        for (auto& m: msg) {
            m_applyChan << m;
        }
        lock.lock();
        SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] applies entries {}-{} in term {}", m_id, m_logs.applied(), last_commit, m_currentTerm);
        // 1. push applyCh 结束之后更新 lastApplied 的时候一定得用之前的 commitIndex ，因为很可能在 push channel 期间发生了改变。
        // 2. 防止与 installSnapshot 并发导致 lastApplied 回退：需要注意到，applier 协程在 push channel 时，中间可能夹杂有 snapshot
        // 也在 push channel。 如果该 snapshot 有效，那么上层状态机和 raft 模块就会原子性的发生替换，即上层状态机更新为 snapshot 的状态，
        // raft 模块更新 log, commitIndex, lastApplied 等等，此时如果这个 snapshot 之后还有一批旧的 entry 在 push channel，
        // 那上层服务需要能够知道这些 entry 已经过时，不能再 apply，同时 applier 这里也应该加一个 Max 自身的函数来防止 lastApplied 出现回退。
        m_logs.appliedTo(std::max(m_logs.applied(), last_commit));
    }
}

void RaftNode::broadcastHeartbeat() {
    for (auto& peer: m_peers) {
        go [id = peer.first, this]{
            replicateOneRound(id);
        };
    }
}


void RaftNode::replicateOneRound(int64_t peer) {
    // 发送 rpc的时候一定不要持锁，否则很可能产生死锁。
    std::unique_lock<MutexType> lock(m_mutex);
    if (m_state != RaftState::Leader) {
        return;
    }

    int64_t prev_index = m_nextIndex[peer] - 1;
    // 该节点的进度远远落后，直接发送快照同步
    if (prev_index < m_logs.lastSnapshotIndex()) {
        InstallSnapshotArgs request{};
        Snapshot::ptr snapshot = m_persister->loadSnapshot();
        if (!snapshot || snapshot->empty()) {
            SPDLOG_LOGGER_ERROR(g_logger, "need non-empty snapshot");
            return;
        }

        SPDLOG_LOGGER_TRACE(g_logger, "Node[{}] [firstIndex: {}, commit: {}] sent snapshot[index: {}, term: {}] to Node[{}]",
                            m_id, m_logs.firstIndex(), m_logs.committed(), snapshot->metadata.index, snapshot->metadata.index, peer);

        request.snapshot = std::move(*snapshot);
        request.term = m_currentTerm;
        request.leaderId = m_id;

        lock.unlock();

        auto reply = m_peers[peer]->installSnapshot(request);
        if (!reply)
            return;

        lock.lock();

        SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] receives InstallSnapshotReply {} from Node[{}] after sending InstallSnapshotArgs {} in term {}",
                            m_id, reply->toString(), peer, request.toString(), m_currentTerm);

        if (m_currentTerm != request.term || m_state != Leader) {
            return;
        }
        // 如果因为网络原因集群选举出新的leader则自己变成follower
        if (reply->term > m_currentTerm) {
            becomeFollower(reply->term, reply->leaderId);
            return;
        }
        if (request.snapshot.metadata.index > m_matchIndex[peer]) {
            m_matchIndex[peer] = request.snapshot.metadata.index;
        }
        if (request.snapshot.metadata.index > m_nextIndex[peer]) {
            m_nextIndex[peer] = request.snapshot.metadata.index + 1;
        }
    } else {
        auto ents = m_logs.entries(m_nextIndex[peer]);
        AppendEntriesArgs request{};
        request.term = m_currentTerm;
        request.leaderId = m_id;
        request.leaderCommit = m_logs.committed();
        request.prevLogIndex = prev_index;
        request.prevLogTerm = m_logs.term(prev_index);
        request.entries = ents;
        lock.unlock();

        auto reply = m_peers[peer]->appendEntries(request);

        if (!reply) {
            return;
        }

        lock.lock();

        SPDLOG_LOGGER_TRACE(g_logger, "Node[{}] receives AppendEntriesReply {} from Node[{}] after sending AppendEntriesArgs {} in term {}",
                            m_id, reply->toString(), peer, request.toString(), m_currentTerm);

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
        // 日志追加失败，根据 nextIndex 更新 m_nextIndex 和 m_matchIndex
        if (!reply->success) {
            if (reply->nextIndex) {
                m_nextIndex[peer] = reply->nextIndex;
                m_matchIndex[peer] = std::min(m_matchIndex[peer], reply->nextIndex - 1);
            }
            return;
        }

        // 日志追加成功，则更新 m_nextIndex 和 m_matchIndex
        if (reply->nextIndex > m_nextIndex[peer]) {
            m_nextIndex[peer] = reply->nextIndex;
            m_matchIndex[peer] = m_nextIndex[peer] - 1;
        }

        int64_t last_index = m_matchIndex[peer];

        // 计算副本数目大于节点数量的一半才提交一个当前任期内的日志
        int64_t vote = 1;
        for (auto match: m_matchIndex) {
            if (match.second >= last_index) {
                ++vote;
            }
            // 假设存在 N 满足N > commitIndex，使得大多数的 matchIndex[i] ≥ N以及log[N].term == currentTerm 成立，
            // 则令 commitIndex = N（5.3 和 5.4 节）
            if (vote > ((int64_t)m_peers.size() + 1) / 2) {
                // 只有领导人当前任期里的日志条目可以被提交
                if (m_logs.maybeCommit(last_index, m_currentTerm)) {
                    m_applyCond.notify_one();
                }
            }
        }
    }
}

RequestVoteReply RaftNode::handleRequestVote(RequestVoteArgs request) {
    std::unique_lock<MutexType> lock(m_mutex);
    RequestVoteReply reply{};
    co_defer_scope {
        persist();
        // 投票后节点的状态
        SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] before processing RequestVoteArgs {} and reply RequestVoteReply {}, state is {}",
                              m_id, request.toString(), reply.toString(), toString());
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
    std::unique_lock<MutexType> lock(m_mutex);
    AppendEntriesReply reply{};
    co_defer_scope {
        if (reply.success && m_logs.committed() < request.leaderCommit) {
           // 更新提交索引，为什么取了个最小？committed是leader发来的，是全局状态，但是当前节点
           // 可能落后于全局状态，所以取了最小值。last_index 是这个节点最新的索引， 不是最大的可靠索引，
           // 如果此时节点异常了，会不会出现commit index以前的日志已经被apply，但是有些日志还没有被持久化？
           // 这里需要解释一下，raft更新了commit index，raft会把commit index以前的日志交给使用者apply同时
           // 会把不可靠日志也交给使用者持久化，所以这要求必须先持久化日志再apply日志，否则就会出现刚刚提到的问题。
           m_logs.commitTo(std::min(request.leaderCommit, m_logs.lastIndex()));
           m_applyCond.notify_one();
        }
        persist();
        SPDLOG_LOGGER_TRACE(g_logger, "Node[{}] before processing AppendEntriesArgs {} and reply AppendEntriesResponse {}, state is {}",
            m_id, request.toString(), reply.toString(), toString());
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

    if (m_leaderId < 0) {
        m_leaderId = request.leaderId;
    }

    // 自己为同一任期内的follower，更新选举定时器就行
    rescheduleElection();

    // 拒绝错误的日志追加请求
    if (request.prevLogIndex < m_logs.lastSnapshotIndex()) {
        reply.term = 0;
        reply.leaderId = -1;
        reply.success = false;
        SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] receives unexpected AppendEntriesArgs {} from Node[{}] because prevLogIndex {} < firstLogIndex {}",
           m_id, request.toString(), request.leaderId, request.prevLogIndex, m_logs.firstIndex());
        return reply;
    }

    int64_t last_index = m_logs.maybeAppend(request.prevLogIndex, request.prevLogTerm, request.leaderCommit, request.entries);

    if (last_index < 0) {
        // 尝试查找冲突的日志
        int64_t conflict = m_logs.findConflict(request.prevLogIndex, request.prevLogTerm);
        reply.term = m_currentTerm;
        reply.leaderId = m_leaderId;
        reply.success = false;
        reply.nextIndex = conflict;
        return reply;
    }

    reply.term = m_currentTerm;
    reply.leaderId = m_leaderId;
    reply.success = true;
    reply.nextIndex = last_index + 1;
    return reply;
}

InstallSnapshotReply RaftNode::handleInstallSnapshot(InstallSnapshotArgs request) {
    std::unique_lock<MutexType> lock(m_mutex);
    InstallSnapshotReply reply{};
    co_defer_scope {
        SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] before processing InstallSnapshotArgs {} and reply InstallSnapshotReply {}, state is {}",
             m_id, request.toString(), reply.toString(), toString());
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
    reply.leaderId = m_leaderId;

    const int64_t snap_index = request.snapshot.metadata.index;
    const int64_t snap_term = request.snapshot.metadata.term;

    // 过时的快照
    if (snap_index <= m_logs.committed()) {
        SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] ignored snapshot [index: {}, term: {}]", m_id, snap_index, snap_term);
        return reply;
    }

    // 快速提交到快照的index
    if (m_logs.matchLog(snap_index, snap_term)) {
        SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] fast-forwarded commit to snapshot [index: {}, term: {}]", m_id, snap_index, snap_term);
        m_logs.commitTo(snap_index);
        m_applyCond.notify_one();
        return reply;
    }

    SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] starts to restore snapshot [index: {}, term: {}]", m_id, snap_index, snap_term);


    if (request.snapshot.metadata.index > m_logs.lastIndex()) {
        // 如果自己的日志太旧了就全部清除了
        m_logs.clearEntries(request.snapshot.metadata.index, request.snapshot.metadata.term);
    } else {
        // 压缩一部分日志
        m_logs.compact(request.snapshot.metadata.index);
    }

    go [snap = request.snapshot, this] {
        m_applyChan << ApplyMsg{snap};
    };
    persist(std::make_shared<Snapshot>(std::move(request.snapshot)));
    return reply;
}

void RaftNode::addPeer(int64_t id, Address::ptr address) {
    RaftPeer::ptr peer = std::make_shared<RaftPeer>(id, address);
    m_peers[id] = peer;
    m_nextIndex[id] = 0;
    m_matchIndex[id] = 0;
    SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] add peer[{}], address is {}", m_id, id, address->toString());
}

bool RaftNode::isLeader() {
    std::unique_lock<MutexType> lock(m_mutex);
    return m_state == Leader;
}

std::pair<int64_t, bool> RaftNode::getState() {
    std::unique_lock<MutexType> lock(m_mutex);
    return {m_currentTerm, m_state == Leader};
}

std::string RaftNode::toString() {
    std::map<RaftState, std::string> mp{{Follower, "Follower"}, {Candidate, "Candidate"}, {Leader, "Leader"}};
    std::string str = fmt::format("Id: {}, State: {}, LeaderId: {}, CurrentTerm: {}, VotedFor: {}, CommitIndex: {}, LastApplied: {}",
                        m_id, mp[m_state], m_leaderId, m_currentTerm, m_votedFor, m_logs.committed(), m_logs.applied());
    return "{" + str + "}";
}

void RaftNode::becomeFollower(int64_t term, int64_t leaderId) {
    // 成为follower停止心跳定时器
    if (m_heartbeatTimer && m_state == RaftState::Leader) {
        m_heartbeatTimer.stop();
    }

    m_state = RaftState::Follower;
    m_currentTerm = term;
    m_votedFor = -1;
    m_leaderId = leaderId;
    persist();
    SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] became follower at term [{}], state is {}", m_id, term, toString());
}

void RaftNode::becomeCandidate() {
    m_state = RaftState::Candidate;
    ++m_currentTerm;
    m_votedFor = m_id;
    m_leaderId = -1;
    persist();
    SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] became Candidate at term {}, state is {}", m_id, m_currentTerm, toString());
}

void RaftNode::becomeLeader() {
    // 成为leader停止选举定时器
    if (m_electionTimer) {
        m_electionTimer.stop();
    }
    m_state = RaftState::Leader;
    m_leaderId = m_id;
    // 成为领导者后，领导者并不知道其它节点的日志情况，因此与其它节点需要同步那么日志，领导者并不知道集群其他节点状态，
    // 因此他选择了不断尝试。nextIndex、matchIndex 分别用来保存其他节点的下一个待同步日志index、已匹配的日志index。
    // nextIndex初始化值为lastIndex+1，即领导者最后一个日志序号+1，因此其实这个日志序号是不存在的，显然领导者也不
    // 指望一次能够同步成功，而是拿出一个值来试探。matchIndex初始化值为0，这个很好理解，因为他还未与任何节点同步成功过，
    // 所以直接为0。
    for (auto& peer : m_peers) {
        m_nextIndex[peer.first] = m_logs.lastIndex() + 1;
        m_matchIndex[peer.first] = 0;
    }
    persist();
    SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] became Leader at term {}, state is {}", m_id, m_currentTerm, toString());

    // note: 即使日志已经被同步到了大多数个节点上，依然不能认为是已经提交了
    // 所以 leader 上任后应该提交一条空日志来提交之前的日志
    Propose("");
    // 成为领导者，发起一轮心跳
    broadcastHeartbeat();
    // 开启心跳定时器
    resetHeartbeatTimer();
}

void RaftNode::rescheduleElection() {
    m_electionTimer.stop();
    m_electionTimer = CycleTimer(GetRandomizedElectionTimeout(), [this] {
        std::unique_lock<MutexType> lock(m_mutex);
        if (m_state != RaftState::Leader) {
            becomeCandidate();
            // 异步投票，不阻塞选举定时器
            startElection();
        }
    });
}

void RaftNode::resetHeartbeatTimer() {
    m_heartbeatTimer.stop();
    m_heartbeatTimer = CycleTimer(GetStableHeartbeatTimeout(), [this] {
        std::unique_lock<MutexType> lock(m_mutex);
        if (m_state == RaftState::Leader) {
            // broadcast
            broadcastHeartbeat();
        }
    });
}

uint64_t RaftNode::GetStableHeartbeatTimeout() {
    return s_timer_heartbeat_ms;
}

uint64_t RaftNode::GetRandomizedElectionTimeout() {
    static std::default_random_engine engine(acid::GetCurrentMS());
    static std::uniform_int_distribution<uint64_t> dist(s_timer_election_base_ms, s_timer_election_top_ms);
    return dist(engine);
}

void RaftNode::persist(Snapshot::ptr snap) {
    HardState hs{};
    hs.vote = m_votedFor;
    hs.term = m_currentTerm;
    hs.commit = m_logs.committed();
    m_persister->persist(hs, m_logs.allEntries(), snap);
}

void RaftNode::persistStateAndSnapshot(int64_t index, const std::string& snap) {
    std::unique_lock<MutexType> lock(m_mutex);
    auto snapshot = m_logs.createSnapshot(index, snap);
    if (snapshot) {
        // 压缩日志
        m_logs.compact(snapshot->metadata.index);
        SPDLOG_LOGGER_DEBUG(g_logger, "starts to restore snapshot [index: {}, term: {}]",
                            snapshot->metadata.index, snapshot->metadata.term);
        persist(snapshot);
    }
}

void RaftNode::persistSnapshot(Snapshot::ptr snapshot) {
    std::unique_lock<MutexType> lock(m_mutex);
    if (snapshot) {
        // 压缩日志
        m_logs.compact(snapshot->metadata.index);
        SPDLOG_LOGGER_DEBUG(g_logger, "starts to restore snapshot [index: {}, term: {}]",
                            snapshot->metadata.index, snapshot->metadata.term);
        persist(snapshot);
    }
}

std::optional<Entry> RaftNode::propose(const std::string& data) {
    std::unique_lock<MutexType> lock(m_mutex);
    return Propose(data);
}

std::optional<Entry> RaftNode::Propose(const std::string &data) {
    if (m_state != Leader) {
        SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] no leader at term {}; dropping proposal", m_id, m_currentTerm);
        return std::nullopt;
    }
    Entry ent;
    ent.term = m_currentTerm;
    ent.index = m_logs.lastIndex() + 1;
    ent.data = data;

    m_logs.append(ent);
    broadcastHeartbeat();
    SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] receives a new log entry[index: {}, term: {}] to replicate in term {}", m_id, ent.index, ent.term, m_currentTerm);
    return ent;
}

}