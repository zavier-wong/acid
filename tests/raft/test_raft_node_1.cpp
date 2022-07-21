//
// Created by zavier on 2022/6/29.
//

#include "acid/raft/raft_node.h"

using namespace acid;
using namespace acid::raft;
static Logger::ptr g_logger = ACID_LOG_ROOT();
std::map<int64_t, std::string> peers = {
        {1, "localhost:9001"},
        {2, "localhost:9002"},
        {3, "localhost:9003"},
};
void raft_server() {
    Storage::ptr storage = std::make_shared<MemoryStorage>();
    RaftLog raftLog(storage);
    Channel<Entry> applyChan(10);
    Channel<std::string> proposeChan(10);
    RaftNode::ptr node = std::make_shared<RaftNode>(1, raftLog, applyChan, proposeChan);
    Address::ptr addr = Address::LookupAny(peers[node->getNodeId()]);
    node->bind(addr);
    for (auto peer: peers) {
        if (peer.first == node->getNodeId())
            continue;
        Address::ptr address = Address::LookupAny(peer.second);
        // 添加节点
        node->addPeer(peer.first, address);
    }
    Go {
        // 接收raft达成共识的日志
        Entry entry;
        while (applyChan >> entry) {
            std::cout << entry.data;
        }
    };
    Go {
        int n = 0;
        while (true) {
            // 发送数据给raft
            proposeChan << std::to_string(n++);
        }
    };
    // 启动raft节点
    node->start();
}

int main() {
    go raft_server;
}