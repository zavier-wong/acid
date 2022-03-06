//
// Created by zavier on 2022/2/16.
//
#include "acid/config.h"
#include "acid/rpc/rpc_client.h"

static acid::Logger::ptr g_logger = ACID_LOG_NAME("system");

namespace acid::rpc {

static ConfigVar<size_t>::ptr g_channel_capacity =
        Config::Lookup<size_t>("rpc.client.channel_capacity",1024,"rpc client channel capacity");

static uint64_t s_channel_capacity = 1;

struct _RpcClientIniter{
    _RpcClientIniter(){
        s_channel_capacity = g_channel_capacity->getValue();
        g_channel_capacity->addListener([](const size_t& old_val, const size_t& new_val){
            ACID_LOG_INFO(g_logger) << "rpc client channel capacity changed from "
                                    << old_val << " to " << new_val;
            s_channel_capacity = new_val;
        });
    }
};

static _RpcClientIniter s_initer;

RpcClient::RpcClient()
    : m_chan(s_channel_capacity){

}

RpcClient::~RpcClient() {
    close();
}

void RpcClient::close() {
    ACID_LOG_DEBUG(g_logger) << "close";
    MutexType::Lock lock(m_mutex);

    if (m_isClose) {
        return;
    }

    m_isHearClose = true;
    m_isClose = true;
    m_chan.close();

    for (auto i: m_responseHandle) {
        i.second << nullptr;
    }

    m_responseHandle.clear();

    if (m_heartTimer) {
        m_heartTimer->cancel();
        m_heartTimer = nullptr;
    }

    IOManager::GetThis()->delEvent(m_session->getSocket()->getSocket(), IOManager::READ);
    if (m_session->isConnected()) {
        m_session->close();
    }
}

bool RpcClient::connect(Address::ptr address){
    Socket::ptr sock = Socket::CreateTCP(address);

    if (!sock) {
        return false;
    }
    if (!sock->connect(address, m_timeout)) {
        m_session = nullptr;
        return false;
    }
    m_isHearClose = false;
    m_isClose = false;
    m_session = std::make_shared<RpcSession>(sock);
    IOManager::GetThis()->submit([this]{
        // 开启 recv 协程
        handleRecv();
    })->submit([this]{
        // 开启 send 协程
        handleSend();
    });

    m_heartTimer = IOManager::GetThis()->addTimer(30'000, [this]{
        ACID_LOG_DEBUG(g_logger) << "heart beat";
        if (m_isHearClose) {
            ACID_LOG_DEBUG(g_logger) << "Server closed";
            close();
        }
        // 创建心跳包
        Protocol::ptr proto = Protocol::Create(Protocol::MsgType::HEARTBEAT_PACKET, "");
        // 向 send 协程的 Channel 发送消息
        m_chan << proto;
        m_isHearClose = true;
    }, true);
    return true;
}

void RpcClient::setTimeout(uint64_t timeout_ms) {
    m_timeout = timeout_ms;
}

void RpcClient::handleSend() {
    Protocol::ptr request;
    // 通过 Channel 收集调用请求，如果没有消息时 Channel 内部会挂起该协程等待消息到达
    // Channel 被关闭时会退出循环
    while (m_chan >> request) {
        if (!request) {
            ACID_LOG_WARN(g_logger) << "RpcClient::handleSend() fail";
            continue;
        }
        // 发送请求
        m_session->sendProtocol(request);
    }
}

void RpcClient::handleRecv() {
    if (!m_session->isConnected()) {
        return;
    }
    while (true) {
        // 接收响应
        Protocol::ptr response = m_session->recvProtocol();
        if (!response) {
            ACID_LOG_WARN(g_logger) << "RpcClient::handleRecv() fail";
            close();
            break;
        }
        Protocol::MsgType type = response->getMsgType();
        // 判断响应类型进行对应的处理
        switch (type) {
            case Protocol::MsgType::HEARTBEAT_PACKET:
                m_isHearClose = false;
                break;
            case Protocol::MsgType::RPC_METHOD_RESPONSE:
                // 处理调用结果
                handleMethodResponse(response);
                break;
            default:
                ACID_LOG_DEBUG(g_logger) << "protocol:" << response->toString();
                break;
        }
    }
}

void RpcClient::handleMethodResponse(Protocol::ptr response) {
    // 获取该调用结果的序列号
    uint32_t id = response->getSequenceId();
    std::map<uint32_t, Channel<Protocol::ptr>>::iterator it;

    MutexType::Lock lock(m_mutex);
    // 查找该序列号的 Channel 是否还存在，如果不存在直接返回
    it = m_responseHandle.find(id);
    if (it == m_responseHandle.end()) {
        return;
    }
    // 通过序列号获取等待该结果的 Channel
    Channel<Protocol::ptr> chan = it->second;
    // 对该 Channel 发送调用结果唤醒调用者
    chan << response;
}

}