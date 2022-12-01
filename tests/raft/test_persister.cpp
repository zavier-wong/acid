//
// Created by zavier on 2022/11/28.
//

#include "acid/raft/persister.h"

using namespace acid::raft;

int main() {
    Persister persister(".");

    HardState hs;
    hs.term = 1;
    hs.vote = 2;
    hs.commit = 3;

    std::vector<Entry> ents;
    Entry entry;
    entry.index = 1;
    entry.term = 2;
    entry.data = "entry";
    ents.push_back(entry);

    Snapshot::ptr snap = std::make_shared<Snapshot>();
    snap->metadata.term = 1;
    snap->metadata.index = 1;
    snap->data = "snap";

    // 持久化存储
    persister.persist(hs, ents, snap);

    if (!persister.loadHardState() || !persister.loadEntries()) {
        SPDLOG_CRITICAL("persist fail, maybe path error");
        return 0;
    }

    hs = *persister.loadHardState();
    ents = *persister.loadEntries();
    snap = persister.loadSnapshot();

    SPDLOG_INFO("hs.term {}, hs.vote {}, hs.commit {}", hs.term, hs.vote, hs.commit);
    for (auto ent: ents) {
        SPDLOG_INFO("entry.index {}, entry.term {}, entry.data {}", entry.index, entry.term, entry.data);
    }
    SPDLOG_INFO("snap->metadata.term {}, snap->metadata.index {}, snap->data {}", snap->metadata.term, snap->metadata.index, snap->data);
}