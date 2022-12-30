//
// Created by zavier on 2022/12/4.
//

#include <random>
#include "../common/config.h"
#include "kvclient.h"

namespace acid::kvraft {
static auto g_logger = GetLogInstance();
// rpc 连接超时时间
static ConfigVar<uint64_t>::ptr g_rpc_timeout =
        Config::Lookup<uint64_t>("kvraft.rpc.timeout", 3000, "kvraft rpc timeout(ms)");

// 连接重试的延时
static ConfigVar<uint32_t>::ptr g_connect_delay =
        Config::Lookup<uint32_t>("kvraft.rpc.reconnect_delay", 2000, "kvraft rpc reconnect delay(ms)");

static uint64_t s_rpc_timeout;
static uint32_t s_connect_delay;

namespace {
    struct KVClientInier{
        KVClientInier(){
            s_rpc_timeout = g_rpc_timeout->getValue();
            g_rpc_timeout->addListener([](const uint64_t& old_val, const uint64_t& new_val){
                SPDLOG_LOGGER_INFO(g_logger, "kvraft rpc timeout changed from {} to {}", old_val, new_val);
                s_rpc_timeout = new_val;
            });
            s_connect_delay = g_connect_delay->getValue();
            g_connect_delay->addListener([](const uint32_t& old_val, const uint32_t& new_val){
                SPDLOG_LOGGER_INFO(g_logger, "kvraft rpc reconnect delay changed from {} to {}", old_val, new_val);
                s_connect_delay = new_val;
            });
        }
    };
    // 初始化配置
    [[maybe_unused]]
    static KVClientInier s_initer;
}

KVClient::KVClient(std::map<int64_t, std::string> &servers) : m_clientId(GetRandom()), m_commandId(0), m_stop(false) {
    for (auto peer: servers) {
        Address::ptr address = Address::LookupAny(peer.second);
        if (!address) {
            SPDLOG_LOGGER_ERROR(g_logger, "lookup server[{}] address fail, address: {}", peer.second, peer.second);
            continue;
        }
        m_servers[peer.first] = address;
    }
    // 设置rpc超时时间
    setTimeout(s_rpc_timeout);
    setHeartbeat(false);
    if (servers.empty()) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "servers empty!");
    } else {
        m_leaderId = m_servers.begin()->first;
    }
    m_stop = false;
}

KVClient::~KVClient() {
    m_heart.stop();
    m_stop = true;
    sleep(5);
}

kvraft::Error KVClient::Get(const std::string& key, std::string& value) {
    CommandRequest request{.operation = GET, .key = key};
    CommandResponse response = Command(request);
    value = response.value;
    return response.error;
}

kvraft::Error KVClient::Put(const std::string& key, const std::string& value) {
    CommandRequest request{.operation = PUT, .key = key, .value = value};
    return Command(request).error;
}

kvraft::Error KVClient::Append(const std::string& key, const std::string& value) {
    CommandRequest request{.operation = APPEND, .key = key, .value = value};
    return Command(request).error;
}

kvraft::Error KVClient::Delete(const std::string& key) {
    CommandRequest request{.operation = DELETE, .key = key};
    return Command(request).error;
}

kvraft::Error KVClient::Clear() {
    CommandRequest request{.operation = CLEAR};
    return Command(request).error;
}

CommandResponse KVClient::Command(CommandRequest& request) {
    request.clientId = m_clientId;
    request.commandId = m_commandId;
    while (!m_stop) {
        CommandResponse response;
        if (!connect()) {
            m_leaderId = nextLeaderId();
            co_sleep(s_connect_delay);
            continue;
        }
        Result<CommandResponse> result = call<CommandResponse>(COMMAND, request);
        if (result.getCode() == RpcState::RPC_SUCCESS) {
            response = result.getVal();
        }
        if (result.getCode() == RpcState::RPC_CLOSED) {
            RpcClient::close();
        }
        if (result.getCode() != RpcState::RPC_SUCCESS || response.error == WRONG_LEADER || response.error == TIMEOUT) {
            if (response.leaderId >= 0) {
                m_leaderId = response.leaderId;
            } else {
                m_leaderId = nextLeaderId();
            }
            RpcClient::close();
            continue;
        }
        ++m_commandId;
        return response;
    }
    return {.error = Error::CLOSED};
}

bool KVClient::connect() {
    if (!isClose()) {
        return true;
    }
    auto address = m_servers[m_leaderId];
    RpcClient::connect(address);
    if (!isClose()) {
        if (m_heart.isCancel()) {
            // 心跳
            m_heart = CycleTimer(3000, [this] {
                [[maybe_unused]]
                std::string dummy;
                Get("", dummy);
            });
        }
        return true;
    }
    return false;
}


int64_t KVClient::GetRandom() {
    static std::default_random_engine engine(acid::GetCurrentMS());
    static std::uniform_int_distribution<int64_t> dist(0, INT64_MAX);
    return dist(engine);
}

int64_t KVClient::nextLeaderId() {
    auto it = m_servers.find(m_leaderId);
    if (it == m_servers.end()) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "leader id {} not exist", m_leaderId);
        return 0;
    }
    ++it;
    if (it == m_servers.end()) {
        return m_servers.begin()->first;
    }
    return it->first;
}

void KVClient::subscribe(PubsubListener::ptr listener, const std::vector<std::string>& channels) {
    {
        std::unique_lock<MutexType> lock(m_pubsub_mutex);
        m_subs = channels;
    }
    while (true) {
        // 连接上 leader
        [[maybe_unused]]
        std::string dummy;
        Get("", dummy);
        std::vector<std::string> vec;
        {
            std::unique_lock<MutexType> lock(m_pubsub_mutex);
            vec = m_subs;
        }
        if (RpcClient::subscribe(listener, vec)) {
            break;
        }
    }
}


void KVClient::unsubscribe(const std::string &channel) {
    {
        std::unique_lock<MutexType> lock(m_pubsub_mutex);
        m_subs.erase(std::remove(m_subs.begin(), m_subs.end(), channel), m_subs.end());
    }
    RpcClient::unsubscribe(channel);
}

void KVClient::patternSubscribe(PubsubListener::ptr listener, const std::vector<std::string>& patterns) {
    {
        std::unique_lock<MutexType> lock(m_pubsub_mutex);
        m_subs = patterns;
    }
    while (true) {
        // 连接上 leader
        [[maybe_unused]]std::string dummy;
        Get("", dummy);
        std::vector<std::string> vec;
        {
            std::unique_lock<MutexType> lock(m_pubsub_mutex);
            vec = m_subs;
        }
        if (RpcClient::patternSubscribe(listener, vec)) {
            break;
        }
    }
}

void KVClient::patternUnsubscribe(const std::string &pattern) {
    {
        std::unique_lock<MutexType> lock(m_pubsub_mutex);
        m_subs.erase(std::remove(m_subs.begin(), m_subs.end(), pattern), m_subs.end());
    }
    RpcClient::patternUnsubscribe(pattern);
}

}