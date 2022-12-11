# 分布式 KV 存储

-------------------

## 引言
注册中心作为服务治理框架的核心，负责所有服务的注册，服务之间的调用也都通过注册中心来请求服务发现。
注册中心重要性不言而喻，一旦宕机，全部的服务都会出现问题，所以我们需要多个注册中心组成集群来提供服务。

本项目中，通过 raft 分布式共识算法，简单实现了分布式一致性的 KV 存储系统，对接口进行了封装，并且提供了 HTTP 接口和 RPC 接口。
为以后注册中心集群的实现打下了基础。

项目链接：https://github.com/zavier-wong/acid

## 用例
在深入源码之前，先简单了解一下使用样例。在 `acid/example/kvhttp` 下提供了一个基于 kvraft 模块实现的简单分布式 kv 存储的前后端。
``` shell
kvhttp
├── CMakeLists.txt          # cmakelists
├── KVHttpServer接口文档.md  # 接口文档
├── index.html              # 前端
└── kvhttp_server.cpp       # 后端
```
先用 cmake 构建 kvhttp，然后使用 `./kvhttp_server 1`，`./kvhttp_server 2`，`./kvhttp_server 3` 来启动三个节点组成一个 kv 集群。

现在双击 index.html 启动前端，就可以通过 web 来于 kv 集群交互。

每个节点都会创建一个 `./kvhttp-x` 目录来存储状态，`./kvhttp-x/raft-state` 存储的是 raft 的持久化数据，当日志的长度大于阈值，
节点会全量序列化当前时刻的数据以快照的形式存储在 `./kvhttp-x/snapshot/` 目录下，并以 raft 的 term 和 index 来命名快照。

这里建议读者先完整运行一遍用例，再继续接下来的学习。

## 设计思路
用例 kvhttp_server 通过 `acid::http::KVStoreServlet` 来处理 http 请求，KVStoreServlet 转发请求给
`acid::kvraft::KVServer` 暴露出来操作 KV 的接口，KVServer 可以看成是一个持有所有已提交的键值对的 map，
并且封装了 `acid::raft::RaftNode`，KVServer 将所有的 KV 操作提交给了 RaftNode，等待操作在集群间达成共识后更新自己的 map。

如下图，KVServer 为 HttpServer 和 RaftNode 之间的通信建立起了桥梁。

```
       1                       2                        3
┌───────────────┐    ┌─────────────────────┐   ┌─────────────────┐
│               │    │  ┌───────────────┐  │   │                 │
│        ◄──────┼────┼──┤               ├──┼───┼─────────►       │
│               │    │  │    RaftNode   │  │   │                 │
│       ────────┼────┼──┤               ├──┼───┼──────────       │
│               │    │  └───┬──────▲────┘  │   │                 │
│               │    │      │      │       │   │                 │
│               │    │  ┌───▼──────┴────┐  │   │                 │
│               │    │  │               │  │   │                 │
│               │    │  │    KVServer   │  │   │                 │
│               │    │  │               │  │   │                 │
│               │    │  └───┬──────▲────┘  │   │                 │
│               │    │      │      │       │   │                 │
│               │    │  ┌───▼──────┴────┐  │   │                 │
│               │    │  │               │  │   │                 │
│               │    │  │   HttpServer  │  │   │                 │
│               │    │  │               │  │   │                 │
│               │    │  └───────────────┘  │   │                 │
└───────────────┘    └─────────────────────┘   └─────────────────┘
```

## KVStoreServlet
`acid::http::KVStoreServlet` 是 Http 服务器的实现。这并不是我们关注的重点。我们需要关注的只是其通过`acid::kvraft::KVServer`中的哪些方法来提供服务。

```cpp
class KVStoreServlet : public Servlet {
public:
    using ptr = std::shared_ptr<KVStoreServlet>;
    KVStoreServlet(std::shared_ptr<acid::kvraft::KVServer> store);
    /**
     * request 和 response 都以 json 来交互
     * request json 格式：
     *  {
     *      "command": "put",
     *      "key": "name",
     *      "value": "zavier"
     *  }
     * response json 格式：
     *  {
     *      "msg": "OK",
     *      "value": "" // 如果 request 的 command 是 get 请求返回对应 key 的 value， 否则为空
     *      "data": {   // 如果 request 的 command 是 dump 请求返回数据库的全部键值对
     *          "key1": "value1",
     *          "key2": "value2",
     *          ...
     *      }
     *  }
     */
    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;
private:
    // KV 存储服务器
    std::shared_ptr<acid::kvraft::KVServer> m_store;
};
```

```cpp

int32_t KVStoreServlet::handle(HttpRequest::ptr request, 
                               HttpResponse::ptr response, 
                               HttpSession::ptr session) {
    nlohmann::json req = request->getJson();
    nlohmann::json resp;
    co_defer_scope {
        response->setJson(resp);
    };
    ...
    Params params(req);
    ...

    const std::string& command = *params.command;

    if (command == "dump") {
        const auto& data = m_store->getData();
        ...
        return 0;
    } else if (command == "clear") {
        kvraft::CommandResponse commandResponse = m_store->Clear();
        ...
        return 0;
    }

    ...
    const std::string& key = *params.key;
    kvraft::CommandResponse commandResponse;
    if (command == "get") {
        commandResponse = m_store->Get(key);
        resp["msg"] = kvraft::toString(commandResponse.error);
        resp["value"] = commandResponse.value;
    } else if (command == "delete") {
        commandResponse = m_store->Delete(key);
        resp["msg"] = kvraft::toString(commandResponse.error);
    } else if (command == "put") {
        ...
        const std::string& value = *params.value;
        commandResponse = m_store->Put(key, value);
        resp["msg"] = kvraft::toString(commandResponse.error);
    } else if (command == "append") {
        ...
        const std::string& value = *params.value;
        commandResponse = m_store->Append(key, value);
        resp["msg"] = kvraft::toString(commandResponse.error);
    } else {
        resp["msg"] = "command not allowed";
    }
    return 0;
}
```

可以看到 KVStoreServlet 只是将请求简单转发给 KVServer。

## KVServer
KVServer 是连接 raft 服务器与 http 服务器的桥梁，是实现键值存储功能的重要组件，但是其实现很简单。

```cpp
class KVServer {
public:
    ...
    using KVMap = std::map<std::string, std::string>;

    KVServer(std::map<int64_t, std::string>& servers, 
             int64_t id, Persister::ptr persister, 
             int64_t maxRaftState = 1000);
    ...
    void start();
    CommandResponse handleCommand(CommandRequest request);
    CommandResponse Get(const std::string& key);
    CommandResponse Put(const std::string& key, const std::string& value);
    CommandResponse Append(const std::string& key, const std::string& value);
    CommandResponse Delete(const std::string& key);
    CommandResponse Clear();
    const KVMap& getData() const { return m_data;}
private:
    void applier();
    void saveSnapshot(int64_t index);
    void readSnapshot(Snapshot::ptr snap);
    ...
    CommandResponse applyLogToStateMachine(const CommandRequest& request);
private:
    ...
    KVMap m_data;
    Persister::ptr m_persister;
    std::unique_ptr<RaftNode> m_raft;
    co::co_chan<raft::ApplyMsg> m_applychan;
    ...
    int64_t m_maxRaftState = -1;
};
```

先看几个字段，m_data 是一个由 map 实现的键值存储，m_persister 是一个持久化管理模块，m_raft 指向了一个 raft 服务器，
m_applychan 是接收 raft 达成共识消息的 channel， m_maxRaftState 是一个阈值，超过之后 KVServer 会生成快照替换日志。

从简单的 start 函数开始看
```cpp
void KVServer::start() {
    readSnapshot(m_persister->loadSnapshot());
    go [this] {
        applier();
    };
    m_raft->start();
}
```
star 里会通过 m_persister 从本地加载一个最近的快照，并调用 readSnapshot 来恢复之前的状态，
然后启动一个协程执行 applier 函数，最后启动 raft 服务器并阻塞在这里。

再看一下在协程里执行的 applier 函数，不断从 m_applychan 里接收 raft 达成共识的消息，再根据消息类型进行对应的操作。
```cpp
void KVServer::applier() {
    ApplyMsg msg{};
    while (m_applychan.pop(msg)) {
        std::unique_lock<MutexType> lock(m_mutex);
        SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] tries to apply message {}", m_id, msg.toString());
        if (msg.type == ApplyMsg::SNAPSHOT) {
            auto snap = std::make_shared<Snapshot>();
            snap->metadata.index = msg.index;
            snap->metadata.term = msg.term;
            snap->data = std::move(msg.data);
            m_raft->persistSnapshot(snap);
            readSnapshot(snap);
            ...
            continue;
        } else if (msg.type == ApplyMsg::ENTRY) {
            // Leader 选出后提交的空日志
            if (msg.data.empty()) {
                continue;
            }
            int64_t msg_idx = msg.index;
            ...
            m_lastApplied = msg_idx;
            auto request = msg.as<CommandRequest>();
            CommandResponse response;
            if (request.operation != GET && isDuplicateRequest(request.clientId, request.commandId)) {
                SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] doesn't apply duplicated message {} to stateMachine because maxAppliedCommandId is {} for client {}",
                                    m_id, request.toString(), m_lastOperation[request.clientId].second.toString(), request.clientId);
                response = m_lastOperation[request.clientId].second;
            } else {
                response = applyLogToStateMachine(request);
                if (request.operation != GET) {
                    m_lastOperation[request.clientId] = {request.commandId, response};
                }
            }
            auto [term, isLeader] = m_raft->getState();
            if (isLeader && msg.term == term) {
                m_nofiyChans[msg.index] << response;
            }
            if (needSnapshot()) {
                saveSnapshot(msg.index);
            }
        } else {
            SPDLOG_LOGGER_CRITICAL(g_logger, "unexpected ApplyMsg type: {}, index: {}, term: {}, data: {}", (int)msg.type, msg.index, msg.term, msg.data);
            exit(EXIT_FAILURE);
        }
    }
}
```

如果接收的消息是 `ApplyMsg::SNAPSHOT`，KVServer 会恢复快照数据。如果接收的消息是 `ApplyMsg::ENTRY`，
说明这条日志已经在集群达成共识，KVServer 将日志反序列化回请求，并判断请求是否写请求且根据 clientId 和 commandId
判断是否在上次执行过，执行过则直接返回上次结果，否则调用 applyLogToStateMachine 函数来 apply 到状态机。
最后通过 `m_nofiyChans[msg.index] << response;` 唤醒等待的协程。

```cpp
CommandResponse KVServer::applyLogToStateMachine(const CommandRequest& request) {
    CommandResponse response;
    KVMap::iterator it;
    switch (request.operation) {
        case GET:
            it = m_data.find(request.key);
            if (it == m_data.end()) {
                response.error = NO_KEY;
            } else {
                response.value = it->second;
            }
            break;
        case PUT:
            m_data[request.key] = request.value;
            break;
        case APPEND:
            m_data[request.key] += request.value;
            break;
        case DELETE:
            it = m_data.find(request.key);
            if (it == m_data.end()) {
                response.error = NO_KEY;
            } else {
                m_data.erase(it);
            }
            break;
        case CLEAR:
            m_data.clear();
            break;
        default:
            SPDLOG_LOGGER_CRITICAL(g_logger, "unexpect operation {}", (int)request.operation);
            exit(EXIT_FAILURE);
    }
    return response;
}
```
applyLogToStateMachine 函数里我们看到了 KV 存储的庐山真面目，就是对一个 map 进行增删改查。

回顾成员函数，会发现这几个相似的函数
```cpp
CommandResponse handleCommand(CommandRequest request);
CommandResponse Get(const std::string& key);
CommandResponse Put(const std::string& key, const std::string& value);
CommandResponse Append(const std::string& key, const std::string& value);
CommandResponse Delete(const std::string& key);
CommandResponse Clear();
```
看一下具体实现
```cpp
CommandResponse KVServer::Get(const std::string& key) {
    CommandRequest request{.operation = GET, .key = key, .commandId = GetRandom()};
    return handleCommand(request);
}

CommandResponse KVServer::Put(const std::string& key, const std::string& value) {
    CommandRequest request{.operation = PUT, .key = key, .value = value, .commandId = GetRandom()};
    return handleCommand(request);
}

CommandResponse KVServer::Append(const std::string& key, const std::string& value) {
    CommandRequest request{.operation = APPEND, .key = key, .value = value, .commandId = GetRandom()};
    return handleCommand(request);
}

CommandResponse KVServer::Delete(const std::string& key) {
    CommandRequest request{.operation = DELETE, .key = key, .commandId = GetRandom()};
    return handleCommand(request);
}

CommandResponse KVServer::Clear() {
    CommandRequest request{.operation = CLEAR, .commandId = GetRandom()};
    return handleCommand(request);
}
```
很容易看出这里为了复用代码减少冗余将所有操作封装成一个 handleCommand 函数
```cpp
CommandResponse KVServer::handleCommand(CommandRequest request) {
    CommandResponse response;
    ...
    if (request.operation != GET && isDuplicateRequest(request.clientId, request.commandId)) {
        response = m_lastOperation[request.clientId].second;
        return response;
    }
    ...
    auto entry = m_raft->propose(request);
    if (!entry) {
        response.error = WRONG_LEADER;
        response.leaderId = m_raft->getLeaderId();
        return response;
    }
    ...
    auto chan = m_nofiyChans[entry->index];
    ...
    if (!chan.TimedPop(response, ...)) {
        response.error = TIMEOUT;
    }
    ...
    m_nofiyChans.erase(entry->index);
    return response;
}
```
在这个函数中会先简单判断一下请求是否重复出现，是的话返回上次结果，否则就提交给了 raft 节点。提交后会判断当前节点是否是集群 leader，不是的话返回错误。
然后通过一个 channel 来等待消息在 raft 间达成共识，如果超时返回错误。

我们可以看到，KVServer 中基本上没有多少与 raft 相关的处理逻辑，大部分代码是对键值存储抽象本身的实现。

## RaftNode
最后，我们来学习 raft 服务器的实现，这也是最重要的部分。
### RaftNode 结构
先来看看结构体中的字段，注释描述了其用途
```cpp
/**
 * @brief Raft 节点，处理 rpc 请求，并改变状态，通过 RaftPeer 调用远端 Raft 节点
 */
class RaftNode : public acid::rpc::RpcServer {
    ...
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
    // 持久化
    Persister::ptr m_persister;
    // 提交已经达成共识的消息
    co::co_condition_variable m_applyCond;
    // 用来已通过raft达成共识的已提交的提议通知给其它组件的信道。
    co::co_chan<ApplyMsg> m_applyChan;
};
```
### RaftNode的创建与启动
在创建 RaftNode 时，需要提供节点 id、持久化管理器 persister、要 apply 到状态机的通道 applyChan这些参数，节点会注册三个 raft 服务的 rpc 调用然后添加其他节点。
```cpp
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
```
在构造函数中，仅初始化了部分参数和注册了一些服务，并没有做过多的操作。现在看一下 start 函数。
```cpp

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
```
start 函数也很简单。首先，尝试从持久化的数据里获取 raft 服务器崩溃前的状态，然后恢复状态，如果没有则重新成为任期为 0 的 follower。
恢复状态后重置选举定时器，开启一个协程执行 applier 函数，然后调用 RpcServer::start 阻塞在这里。

在 applier 协程里，raft 阻塞获取需要 apply 的日志，然后发送到 m_applyChan 里通知上层服务日志可以 apply，最后将日志提交到该 index。
```cpp
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

        ...
        for (auto& m: msg) {
            m_applyChan << m;
        }
        ...
        // 1. push applyCh 结束之后更新 lastApplied 的时候一定得用之前的 commitIndex ，因为很可能在 push channel 期间发生了改变。
        // 2. 防止与 installSnapshot 并发导致 lastApplied 回退：需要注意到，applier 协程在 push channel 时，中间可能夹杂有 snapshot
        // 也在 push channel。 如果该 snapshot 有效，那么上层状态机和 raft 模块就会原子性的发生替换，即上层状态机更新为 snapshot 的状态，
        // raft 模块更新 log, commitIndex, lastApplied 等等，此时如果这个 snapshot 之后还有一批旧的 entry 在 push channel，
        // 那上层服务需要能够知道这些 entry 已经过时，不能再 apply，同时 applier 这里也应该加一个 Max 自身的函数来防止 lastApplied 出现回退。
        m_logs.appliedTo(std::max(m_logs.applied(), last_commit));
    }
}
```

### Raft Leader 选举

在 leader 选举方面有许多种优化算法，目前精力有限并没有进行优化，但以后会慢慢加上。

在 star 函数里我们重置了选举定时器，超时后将会调用 startElection 向全部节点发起选举投票。
要注意的是，为了防止节点间超时时间相同一起发起选举然后瓜分选票，这里需要从 GetRandomizedElectionTimeout 获取随机超时。

```cpp
void RaftNode::rescheduleElection() {
    ...
    m_electionTimer = CycleTimer(GetRandomizedElectionTimeout(), [this] {
        ...
        if (m_state != RaftState::Leader) {
            becomeCandidate();
            // 异步投票，不阻塞选举定时器
            startElection();
        }
    });
}
```
startElection 是选举的核心，这里的 requestVote 是对 rpc 的封装，节点使用协程发起异步投票，不阻塞选举定时器，这样在选举超时后能发起新的选举。
在协程间通过 shared_ptr 来共享本轮票数，如果票数过半则成功竞选为 leader，否则重新等待选举定时器超时。

注意，在投票前需要先持久化状态。
```cpp
void RaftNode::startElection() {
    RequestVoteArgs request{};
    request.term = m_currentTerm;
    request.candidateId = m_id;
    request.lastLogIndex = m_logs.lastIndex();
    request.lastLogTerm = m_logs.lastTerm();
    ...
    m_votedFor = m_id;
    persist();
    // 统计投票结果，自己开始有一票
    std::shared_ptr<int64_t> grantedVotes = std::make_shared<int64_t>(1);

    for (auto& peer: m_peers) {
        // 使用协程发起异步投票，不阻塞选举定时器，才能在选举超时后发起新的选举
        go [grantedVotes, request, peer, this] {
            auto reply = peer.second->requestVote(request);
            ...
            // 检查自己状态是否改变
            if (m_currentTerm == request.term && m_state == RaftState::Candidate) {
                if (reply->voteGranted) {
                    ++(*grantedVotes);
                    // 赢得选举
                    if (*grantedVotes > ((int64_t)m_peers.size() + 1) / 2) {
                        ...
                        becomeLeader();
                    }
                } else if (reply->term > m_currentTerm) {
                    ...
                    becomeFollower(reply->term, reply->leaderId);
                    rescheduleElection();
                }
            }
        };
    }
}
```
当前节点在 handleRequestVote 里处理别的节点发起的投票请求。

首先会进行一系列检查，只有通过检查的才能给节点投票，然后重置选举定时器。

重点，成功投票后才重置选举定时器，这样有助于网络不稳定条件下选主的 liveness 问题。

例如：极端情况下，Candidate 只能发送消息而收不到 Follower 的消息，Candidate 会不断发起新
一轮的选举，如果当前节点在 becomeFollower 后马上重置选举定时器，会导致节点无法发起选举，该集群
无法选出新的 Leader。

只是重置定时器的时机不一样，就导致了集群无法容忍仅仅 1 个节点故障
```cpp

RequestVoteReply RaftNode::handleRequestVote(RequestVoteArgs request) {
    std::unique_lock<MutexType> lock(m_mutex);
    RequestVoteReply reply{};
    co_defer_scope {
        persist();
        ...
    };
    // 拒绝给任期小于自己的候选人投票
    if (request.term < m_currentTerm ||
            // 多个候选人发起选举的情况
            (request.term == m_currentTerm && m_votedFor != -1 && m_votedFor != request.candidateId)) {
        ...
        return reply;
    }

    // 任期比自己大，变成追随者
    if (request.term > m_currentTerm) {
        becomeFollower(request.term);
    }

    // 拒绝掉那些日志没有自己新的投票请求
    if (!m_logs.isUpToDate(request.lastLogIndex, request.lastLogTerm)) {
        ...
        return reply;
    }

    // 投票给候选人
    m_votedFor = request.candidateId;

    ...
    rescheduleElection();

    // 返回给候选人的结果
    ...
    return reply;
}
```

### Raft 日志复制与快照安装
raft 的快照复制与快照安装都是由 leader 发起的。
当节点 becomeLeader 后，会开启心跳定时器
```cpp
void RaftNode::becomeLeader() {
    ...
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
    ...
    // note: 即使日志已经被同步到了大多数个节点上，依然不能认为是已经提交了
    // 所以 leader 上任后应该提交一条空日志来提交之前的日志
    Propose("");
    // 成为领导者，发起一轮心跳
    broadcastHeartbeat();
    // 开启心跳定时器
    resetHeartbeatTimer();
}
```
心跳定时器超时后会调用 broadcastHeartbeat 广播心跳。
```cpp
void RaftNode::resetHeartbeatTimer() {
    ...
    m_heartbeatTimer = CycleTimer(GetStableHeartbeatTimeout(), [this] {
        ...
        if (m_state == RaftState::Leader) {
            // broadcast
            broadcastHeartbeat();
        }
    });
}
```
broadcastHeartbeat 里对每个节点开启一个协程调用 replicateOneRound 来日志复制或快照安装。
```cpp
void RaftNode::broadcastHeartbeat() {
    for (auto& peer: m_peers) {
        go [id = peer.first, this]{
            replicateOneRound(id);
        };
    }
}
```
replicateOneRound 分成两部分来看。

首先获取该节点的下一条日志 index，如果小于自己快照的最后 index，说明该节点的进度远远落后，直接发送快照同步。
```cpp
void RaftNode::replicateOneRound(int64_t peer) {
    ...
    int64_t prev_index = m_nextIndex[peer] - 1;
    // 该节点的进度远远落后，直接发送快照同步
    if (prev_index < m_logs.lastSnapshotIndex()) {
        InstallSnapshotArgs request{};
        Snapshot::ptr snapshot = m_persister->loadSnapshot();
        ...
        request.snapshot = std::move(*snapshot);
        ...
        auto reply = m_peers[peer]->installSnapshot(request);
        ...
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
        ...
    }
}
```
节点收到安装快照的请求会调用 handleInstallSnapshot 来处理。

先进行快照的合法性检查，通过检查后压缩自己的日志，并持久化快照。同时会发起一个协程向 m_applyChan 发送快照消息，让上层应用来决定快照如何使用。

```cpp
InstallSnapshotReply RaftNode::handleInstallSnapshot(InstallSnapshotArgs request) {
    std::unique_lock<MutexType> lock(m_mutex);
    InstallSnapshotReply reply{};
    ...

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
        ...
        return reply;
    }

    // 快速提交到快照的index
    if (m_logs.matchLog(snap_index, snap_term)) {
        ...
        m_logs.commitTo(snap_index);
        m_applyCond.notify_one();
        return reply;
    }
    
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
```
replicateOneRound 的另一部分就是日志复制，对该节点发起日志追加请求后，会收到节点的 nextIndex，这是用来日志优化的，可以快速回退到该 index。

在最后会进行检查日志是否已经复制到大部分节点上，然后提交该 index。
```cpp
void RaftNode::replicateOneRound(int64_t peer) {
    ...
    int64_t prev_index = m_nextIndex[peer] - 1;
    // 该节点的进度远远落后，直接发送快照同步
    if (prev_index < m_logs.lastSnapshotIndex()) {
        ...
    } else {
        auto ents = m_logs.entries(m_nextIndex[peer]);
        AppendEntriesArgs request{};
        request.term = m_currentTerm;
        request.leaderId = m_id;
        request.leaderCommit = m_logs.committed();
        request.prevLogIndex = prev_index;
        request.prevLogTerm = m_logs.term(prev_index);
        request.entries = ents;
        ...

        auto reply = m_peers[peer]->appendEntries(request);
        ...
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
```

节点收到日志追加的请求会调用 handleAppendEntries 来处理。

所有与日志有关的操作都封装到 RaftLog 里，这里会调用 maybeAppend 尝试追加日志，
追加失败会查找冲突日志来快速回退。最后会提交日志并唤醒 applier 协程。

```cpp

AppendEntriesReply RaftNode::handleAppendEntries(AppendEntriesArgs request) {
    std::unique_lock<MutexType> lock(m_mutex);
    AppendEntriesReply reply{};
    co_defer_scope {
        persist();
        if (reply.success && m_logs.committed() < request.leaderCommit) {
           // 更新提交索引，为什么取了个最小？committed是leader发来的，是全局状态，但是当前节点
           // 可能落后于全局状态，所以取了最小值。last_index 是这个节点最新的索引， 不是最大的可靠索引，
           // 如果此时节点异常了，会不会出现commit index以前的日志已经被apply，但是有些日志还没有被持久化？
           // 这里需要解释一下，raft更新了commit index，raft会把commit index以前的日志交给使用者apply同时
           // 会把不可靠日志也交给使用者持久化，所以这要求必须先持久化日志再apply日志，否则就会出现刚刚提到的问题。
           m_logs.commitTo(std::min(request.leaderCommit, m_logs.lastIndex()));
           m_applyCond.notify_one();
        }
        ...
    };
    // 拒绝任期小于自己的 leader 的日志复制请求
    if (request.term < m_currentTerm) {
        ...
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
        ...
        return reply;
    }

    int64_t last_index = 
            m_logs.maybeAppend(request.prevLogIndex, request.prevLogTerm, request.leaderCommit, request.entries);

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
```
## 最后
本文简单介绍了 kv 的整体结构，忽略了很多细节，旨在帮助读者理清思路更好地阅读源码。