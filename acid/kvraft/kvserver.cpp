//
// Created by zavier on 2022/12/3.
//
#include <random>
#include "kvserver.h"
#include "../common/config.h"

namespace acid::kvraft {
using namespace acid;
static auto g_logger = GetLogInstance();
static int64_t GetRandom() {
    static std::default_random_engine engine(acid::GetCurrentMS());
    static std::uniform_int_distribution<int64_t> dist(0, INT64_MAX);
    return dist(engine);
}

KVServer::KVServer(std::map<int64_t, std::string>& servers, int64_t id, Persister::ptr persister, int64_t maxRaftState)
    : m_id(id)
    , m_persister(persister)
    , m_maxRaftState(maxRaftState) {
    Address::ptr addr = Address::LookupAny(servers[id]);
    m_raft = std::make_unique<RaftNode>(servers, id, persister, m_applychan);
    while (!m_raft->bind(addr)) {
        SPDLOG_LOGGER_WARN(g_logger, "kvserver[{}] bind {} fail", id, addr->toString());
        sleep(3);
    }
    m_raft->registerMethod(COMMAND, [this](CommandRequest request) {
        return handleCommand(std::move(request));
    });
}

KVServer::~KVServer() {
    stop();
}

void KVServer::start() {
    readSnapshot(m_persister->loadSnapshot());
    go [this] {
        applier();
    };
    m_raft->start();
}

void KVServer::stop() {
    std::unique_lock<MutexType> lock(m_mutex);
    m_raft->stop();
}

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

CommandResponse KVServer::handleCommand(CommandRequest request) {
    CommandResponse response;
    co_defer_scope {
        SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] processes CommandRequest {} with CommandResponse {}",
                            m_id, request.toString(), response.toString());
    };
    std::unique_lock<MutexType> lock(m_mutex);
    if (request.operation != GET && isDuplicateRequest(request.clientId, request.commandId)) {
        response = m_lastOperation[request.clientId].second;
        return response;
    }
    lock.unlock();
    auto entry = m_raft->propose(request);
    if (!entry) {
        response.error = WRONG_LEADER;
        response.leaderId = m_raft->getLeaderId();
        return response;
    }
    lock.lock();
    auto chan = m_nofiyChans[entry->index];
    lock.unlock();

    if (!chan.TimedPop(response, std::chrono::milliseconds(acid::Config::Lookup<uint64_t>("raft.rpc.timeout")->getValue()))) {
        response.error = TIMEOUT;
    }
    lock.lock();
    m_nofiyChans.erase(entry->index);
    if (response.error == Error::OK) {
        switch (request.operation) {
            case PUT:
                go [key = request.key, this] {
                    m_raft->publish(TOPIC_KEYEVENTS_PUT, key);
                    m_raft->publish(TOPIC_KEYSPACE + key, KEYEVENTS_PUT);
                };
                break;
            case APPEND:
                go [key = request.key, this] {
                    m_raft->publish(TOPIC_KEYEVENTS_APPEND, key);
                    m_raft->publish(TOPIC_KEYSPACE + key, KEYEVENTS_APPEND);
                };
                break;
            case DELETE:
                go [key = request.key, this] {
                    m_raft->publish(TOPIC_KEYEVENTS_DEL, key);
                    m_raft->publish(TOPIC_KEYSPACE + key, KEYEVENTS_DEL);
                };
                break;
            default:
                void(0);
        }
    }
    return response;
}

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
            m_lastApplied = msg.index;
            continue;
        } else if (msg.type == ApplyMsg::ENTRY) {
            // Leader 选出后提交的空日志
            if (msg.data.empty()) {
                continue;
            }
            int64_t msg_idx = msg.index;
            if (msg_idx <= m_lastApplied) {
                SPDLOG_LOGGER_DEBUG(g_logger, "Node[{}] discards outdated message {} because a newer snapshot which lastApplied is {} has been restored",
                                    m_id, msg.toString(), m_lastApplied);
                continue;
            }
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

void KVServer::saveSnapshot(int64_t index) {
    Serializer s;
    s << m_data << m_lastOperation;
    s.reset();
    m_raft->persistStateAndSnapshot(index, s.toString());
}

void KVServer::readSnapshot(Snapshot::ptr snap) {
    if (!snap) {
        return;
    }
    Serializer s(snap->data);
    try {
        m_data.clear();
        m_lastOperation.clear();
        s >> m_data >> m_lastOperation;
    } catch (...) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "KVServer[{}] read snapshot fail", m_id);
    }
}

bool KVServer::isDuplicateRequest(int64_t client, int64_t command) {
    auto it = m_lastOperation.find(client);
    if (it == m_lastOperation.end()) {
        return false;
    }
    return it->second.first == command;
}

bool KVServer::needSnapshot() {
    if (m_maxRaftState == -1) {
        return false;
    }
    return m_persister->getRaftStateSize() >= m_maxRaftState;
}

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

}