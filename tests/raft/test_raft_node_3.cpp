//
// Created by zavier on 2022/11/30.
//

#include "acid/raft/raft_node.h"

using namespace acid;
using namespace acid::raft;

std::map<int64_t, std::string> peers = {
        {1, "localhost:7001"},
        {2, "localhost:7002"},
        {3, "localhost:7003"},
};

void Main() {
    int64_t id = 3;
    Persister::ptr persister = std::make_shared<Persister>(fmt::format("raft-node-{}", id));
    co::co_chan<ApplyMsg> applyChan;
    RaftNode node(peers, id, persister, applyChan);
    Address::ptr addr = Address::LookupAny(peers[node.getNodeId()]);
    node.bind(addr);
    go [&node, id] {
        for (int i = 0; ; ++i) {
            if (node.isLeader()) {
                node.propose(fmt::format("Node[{}] propose {}", id, i));
            }
            sleep(5);
        }
    };
    go [applyChan] {
        // 接收raft达成共识的日志
        ApplyMsg msg;
        while (applyChan.pop(msg)) {
            switch (msg.type) {
                case ApplyMsg::ENTRY:
                    SPDLOG_INFO("entry-> index: {}, term: {}, data: {}", msg.index, msg.term, msg.data);
                    break;
                case ApplyMsg::SNAPSHOT:
                    SPDLOG_INFO("snapshot-> index: {}, term: {}, data: {}", msg.index, msg.term, msg.data);
                    break;
            }
        }
    };
    // 启动raft节点
    node.start();
}

int main() {
    go Main;
    co_sched.Start();
}