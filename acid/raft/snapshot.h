//
// Created by zavier on 2022/7/8.
//

#ifndef ACID_SNAPSHOT_H
#define ACID_SNAPSHOT_H

#include <filesystem>
#include "../rpc/serializer.h"

namespace acid::raft {

/**
 * @brief 快照元数据
 */
struct SnapshotMetadata {
    // 快照中包含的最后日志条目的索引值
    int64_t index;
    // 快照中包含的最后日志条目的任期号
    int64_t term;
    friend rpc::Serializer& operator<<(rpc::Serializer& s, const SnapshotMetadata& snap) {
        s << snap.index << snap.term;
        return s;
    }
    friend rpc::Serializer& operator>>(rpc::Serializer& s, SnapshotMetadata& snap) {
        s >> snap.index >> snap.term;
        return s;
    }
};

/**
 * @brief 快照
 */
struct Snapshot {
    using ptr = std::shared_ptr<Snapshot>;
    // 使用者某一时刻的全量序列化后的数据
    SnapshotMetadata metadata;
    std::string data;
    bool empty() const {
        return metadata.index == 0;
    }
    friend rpc::Serializer& operator<<(rpc::Serializer& s, const Snapshot& snap) {
        s << snap.metadata << snap.data;
        return s;
    }
    friend rpc::Serializer& operator>>(rpc::Serializer& s, Snapshot& snap) {
        s >> snap.metadata >> snap.data;
        return s;
    }
};

/**
 * @brief 快照管理器，复制快照的存储和加载
 */
class Snapshotter {
public:
    Snapshotter(const std::filesystem::path& dir, const std::string& snap_suffix = ".snap");
    /**
    * @brief 对外暴露的接口，存储并持久化一个snapshot
    * @return bool 是否成功存储
    */
    bool saveSnap(const Snapshot::ptr& snapshot);
    /**
    * @brief 对外暴露的接口，存储并持久化一个snapshot
    * @return bool 是否成功存储
    */
    bool saveSnap(const Snapshot& snapshot);
    /**
    * @brief 加载最新的一个快照
    * @return Snapshot::ptr 如果没有快照则返回nullptr
    */
    Snapshot::ptr loadSnap();
private:
    /**
    * @brief 按逻辑顺序（最新的快照到旧快照）返回快照列表
    * @return std::vector<std::string> 快照列表
    */
    std::vector<std::string> snapNames();
    /**
    * @brief 对文件名后缀的合法性检查
     * @param names 文件名列表
    * @return 合法的快照名列表
    */
    std::vector<std::string> checkSffix(const std::vector<std::string>& names);
    /**
    * @brief 将snapshot序列化后持久化到磁盘
    */
    bool save(const Snapshot& snapshot);
    /**
    * @brief 反序列化成 snapshot
    */
    std::unique_ptr<Snapshot> read(const std::string& snapname);
private:
    // 快照目录
    const std::filesystem::path m_dir;
    // 快照文件后缀
    const std::string m_snap_suffix;
};

}
#endif //ACID_SNAPSHOT_H
