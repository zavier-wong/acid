//
// Created by zavier on 2022/11/28.
//

#include <fstream>
#include "persister.h"

namespace acid::raft {
static auto g_logger = GetLogInstance();

Persister::Persister(const std::filesystem::path& path) : m_path(path), m_shotter(path / "snapshot") {
    if (m_path.empty()) {
        SPDLOG_LOGGER_WARN(g_logger, "Persist path is empty");
    } else if (!std::filesystem::exists(m_path)) {
        SPDLOG_LOGGER_WARN(g_logger, "Persist path: {} is not exists, create directory", m_path.string());
        std::filesystem::create_directories(path);
    } else if (!std::filesystem::is_directory(m_path)) {
        SPDLOG_LOGGER_WARN(g_logger, "Persist path: {} is not a directory", m_path.string());
    } else {
        SPDLOG_LOGGER_INFO(g_logger, "Persist path : {}", getFullPathName());
    }
}

std::optional<HardState> Persister::loadHardState() {
    std::ifstream in(m_path / m_name, std::ios_base::in);
    if (!in.is_open()) {
        return std::nullopt;
    }
    std::string str((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    rpc::Serializer s(str);
    HardState hs{};
    try {
        s >> hs;
    } catch (...) {
        return std::nullopt;
    }
    return hs;
}

std::optional<std::vector<Entry>> Persister::loadEntries() {
    std::unique_lock<co::co_mutex> lock(m_mutex);
    std::ifstream in(m_path / m_name, std::ios_base::in);
    if (!in.is_open()) {
        return std::nullopt;
    }
    std::string str((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    rpc::Serializer s(str);
    std::vector<Entry> ents;
    try {
        HardState hs{};
        s >> hs >> ents;
    } catch (...) {
        return std::nullopt;
    }
    return ents;
}

Snapshot::ptr Persister::loadSnapshot() {
    std::unique_lock<co::co_mutex> lock(m_mutex);
    return m_shotter.loadSnap();
}

int64_t Persister::getRaftStateSize() {
    std::unique_lock<co::co_mutex> lock(m_mutex);
    std::ifstream in(m_path / m_name, std::ios_base::in);
    if (!in.is_open()) {
        return -1;
    }
    in.seekg(0, in.end);
    auto fos = in.tellg();
    return fos;
}

bool Persister::persist(const HardState &hs, const std::vector <Entry> &ents, const Snapshot::ptr snapshot) {
    std::unique_lock<co::co_mutex> lock(m_mutex);
    int fd = open((m_path / m_name).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return false;
    }

    rpc::Serializer s;
    s << hs << ents;
    s.reset();

    std::string data = s.toString();
    if (write(fd, data.c_str(), data.size()) < 0) {
        return false;
    }
    // 持久化，必须马上刷新磁盘
    fsync(fd);
    close(fd);
    if (snapshot) {
        return m_shotter.saveSnap(snapshot);
    }
    return true;
}

}