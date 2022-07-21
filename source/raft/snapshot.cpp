//
// Created by zavier on 2022/7/8.
//

#include <fcntl.h>
#include <unistd.h>
#include "acid/raft/snapshot.h"
#include "acid/log.h"

namespace acid::raft {
static Logger::ptr g_logger = ACID_LOG_NAME("raft");

Snapshotter::Snapshotter(const std::filesystem::path& dir,
                         const std::string& snap_suffix /* = ".snap"*/)
        : m_dir(dir)
        , m_snap_suffix(snap_suffix) {
    if (dir.empty()) {
        ACID_LOG_WARN(g_logger) << "snapshot path is empty";
    }
    if (!std::filesystem::exists(m_dir)) {
        ACID_LOG_WARN(g_logger) << "snapshot path: " << m_dir << " is not exists";
    }
    if (!std::filesystem::is_directory(m_dir)) {
        ACID_LOG_WARN(g_logger) << "snapshot path: " << m_dir << " is not a directory";
    }
}

bool Snapshotter::saveSnap(Snapshot::ptr snapshot) {
    if (!snapshot || snapshot->empty()) {
        return false;
    }
    return save(snapshot);
}

Snapshot::ptr Snapshotter::loadSnap() {
    std::vector<std::string> names = snapNames();
    if (names.empty()) {
        return nullptr;
    }
    // 从最新到最旧来遍历所有snapshot文件
    for (auto& name: names) {
        std::unique_ptr<Snapshot> snap = read(name);
        if (snap) {
            // 成功加载快照就返回
            return snap;
        }
    }
    return nullptr;
}

std::vector<std::string> Snapshotter::snapNames() {
    std::vector<std::string> snaps;
    if (!std::filesystem::exists(m_dir) || !std::filesystem::is_directory(m_dir)) {
        return {};
    }
    std::filesystem::directory_iterator dirite(m_dir);
    for (auto& ite : dirite) {
        // 忽略其他类型文件
        if (ite.status().type() == std::filesystem::file_type::regular) {
            snaps.push_back(ite.path().filename().string());
        }
    }
    snaps = checkSffix(snaps);
    std::sort(snaps.begin(), snaps.end(), std::greater<>());
    return snaps;
}

std::vector<std::string> Snapshotter::checkSffix(const std::vector<std::string>& names) {
    std::vector<std::string> snaps;
    for (auto &name: names) {
        // 检查文件后缀
        if (name.compare(name.size() - m_snap_suffix.size(), m_snap_suffix.size(), m_snap_suffix) == 0) {
            snaps.push_back(name);
        } else {
            ACID_LOG_WARN(g_logger) << "skipped unexpected non snapshot file " << name;
        }
    }
    return snaps;
}

bool Snapshotter::save(Snapshot::ptr snapshot) {
    // 快照名格式 %016ld-%016ld%s
    std::unique_ptr<char[]> buff = std::make_unique<char[]>(16 + 1 + 16 + m_snap_suffix.size() + 1);
    sprintf(&buff[0],"%016ld-%016ld%s", snapshot->metadata.index, snapshot->metadata.index, m_snap_suffix.c_str());
    std::string filename = m_dir / (&buff[0]);

    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);

    rpc::Serializer ser;
    ser << (*snapshot);
    ser.reset();

    std::string data = ser.toString();
    if (write(fd, data.c_str(), data.size()) < 0) {
        return false;
    }
    // 持久化，必须马上刷新磁盘
    fsync(fd);
    close(fd);
    return true;
}

std::unique_ptr<Snapshot> Snapshotter::read(const std::string& snapname) {
    std::ifstream file(m_dir / snapname, std::ios::binary);
    file.seekg(0, file.end);
    size_t length = file.tellg();
    file.seekg(0, file.beg);

    if (!length) {
        return nullptr;
    }

    std::string data;
    data.resize(length);
    file.read(&data[0], length);

    rpc::Serializer ser(data);
    std::unique_ptr<Snapshot> snapshot = std::make_unique<Snapshot>();
    ser >> (*snapshot);
    return snapshot;
}
}