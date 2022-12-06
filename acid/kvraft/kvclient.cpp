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

// rpc 连接重试次数
static ConfigVar<uint32_t>::ptr g_connect_retry =
        Config::Lookup<uint32_t>("kvraft.rpc.connect_retry", 3, "kvraft rpc connect retry times");

static uint64_t s_rpc_timeout;
static uint32_t s_connect_retry;

namespace {
    struct KVClientInier{
        KVClientInier(){
            s_rpc_timeout = g_rpc_timeout->getValue();
            g_rpc_timeout->addListener([](const uint64_t& old_val, const uint64_t& new_val){
                SPDLOG_LOGGER_INFO(g_logger, "kvraft rpc timeout changed from {} to {}", old_val, new_val);
                s_rpc_timeout = new_val;
            });
            s_connect_retry = g_connect_retry->getValue();
            g_rpc_timeout->addListener([](const uint32_t& old_val, const uint32_t& new_val){
                SPDLOG_LOGGER_INFO(g_logger, "kvraft rpc timeout changed from {} to {}", old_val, new_val);
                s_connect_retry = new_val;
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
        // 关闭 RpcClient 自带的心跳
        auto server = std::make_shared<rpc::RpcClient>();
        // 关闭心跳
        server->setHeartbeat(false);
        // 设置rpc超时时间
        server->setTimeout(s_rpc_timeout);
        m_servers[peer.first] = {address, server};
    }
    if (servers.empty()) {
        SPDLOG_LOGGER_CRITICAL(g_logger, "servers empty!");
    } else {
        m_leaderId = m_servers.begin()->first;
    }
    m_stop = false;
}

KVClient::~KVClient() {
    m_stop = true;
    m_stopChan >> nullptr;
}

std::string KVClient::Get(const std::string& key) {
    CommandRequest request{.operation = GET, .key = key};
    return Command(request).value;
}

void KVClient::Put(const std::string& key, const std::string& value) {
    CommandRequest request{.operation = PUT, .key = key, .value = value};
    Command(request);
}

void KVClient::Append(const std::string& key, const std::string& value) {
    CommandRequest request{.operation = APPEND, .key = key, .value = value};
    Command(request);
}

bool KVClient::Delete(const std::string& key) {
    CommandRequest request{.operation = DELETE, .key = key};
    return Command(request).error == Error::OK;
}

CommandResponse KVClient::Command(CommandRequest& request) {
    request.clientId = m_clientId;
    request.commandId = m_commandId;
    while (!m_stop) {
        CommandResponse response;
        if (!connect(m_leaderId)) {
            m_leaderId = nextLeaderId();
            co_sleep(100);
            continue;
        }
        auto server = m_servers[m_leaderId].second;
        Result<CommandResponse> result = server->call<CommandResponse>(COMMAND, request);
        if (result.getCode() == RpcState::RPC_SUCCESS) {
            response = result.getVal();
        }
        if (result.getCode() == RpcState::RPC_CLOSED) {
            server->close();
        }
        if (result.getCode() != RpcState::RPC_SUCCESS || response.error == WRONG_LEADER || response.error == TIMEOUT) {
            if (response.leaderId >= 0) {
                m_leaderId = response.leaderId;
            } else {
                m_leaderId = nextLeaderId();
            }
            continue;
        }
        ++m_commandId;
        return response;
    }
    m_stopChan << true;
    return {};
}

bool KVClient::connect(int64_t id) {
    auto server = m_servers[id].second;
    auto address = m_servers[id].first;

    if (!server->isClose()) {
        return true;
    }
    for (int i = 1; i <= (int)s_connect_retry; ++i) {
        // 重连 Raft 节点
        server->connect(address);
        if (!server->isClose()) {
            return true;
        }
        co_sleep(10 * i);
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


}