//
// Created by zavier on 2022/4/11.
//

#include "acid/file_monitor.h"
#include "acid/io_manager.h"

namespace acid {

FileMonitor::FileMonitor() {
    m_inotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
}

FileMonitor::~FileMonitor() {
    IOManager::GetThis()->cancelAllEvent(m_inotify);
    close(m_inotify);
}

bool FileMonitor::addFileWatch(const std::string& pathname, uint32_t mask, std::function<void(EventContext)> cb) {
    auto warp = [cb](std::vector<EventContext> vec) {
        cb(vec[0]);
    };
    return addWatch(pathname, mask, warp);
}

bool FileMonitor::addDirectoryWatch(const std::string& pathname, uint32_t mask,
                       std::function<void(std::vector<EventContext>)> cb) {
    return addWatch(pathname, mask, cb);
}

bool FileMonitor::addWatch(const std::string& pathname, uint32_t mask,
              std::function<void(std::vector<EventContext>)> cb) {
    delWatch(pathname);
    MutexType::Lock lock(m_mutex);
    int wd = inotify_add_watch(m_inotify, pathname.c_str(), mask);
    if (wd == -1) return false;
    m_map[pathname] = wd;
    m_cbs[wd] = {pathname, std::move(cb)};
    if (m_map.size() == 1) {
        go [this] {
            std::unique_ptr<char[]> buff(new char[4096]);
            do {
                ssize_t n = read(m_inotify, buff.get(), 4096);
                while (n == -1 && errno == EINTR){
                    n = read(m_inotify, buff.get(), 4096);
                }
                if(n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)){
                    IOManager* ioManager = IOManager::GetThis();
                    int rt = ioManager->addEvent(m_inotify, IOManager::READ);
                    if(!rt){
                        return -1;
                    }
                    Fiber::YieldToHold();
                    continue;
                }

                if (n < 0) {
                    break;
                }

                const struct inotify_event *event;
                std::map<int, std::vector<EventContext>> mp;
                for (char* ptr = buff.get(); ptr < buff.get() + n;
                     ptr += sizeof(struct inotify_event) + event->len) {
                    event = (const struct inotify_event *) ptr;
                    std::string name(event->name, event->len);
                    MutexType::Lock lock(m_mutex);
                    std::string path = m_cbs[event->wd].first;
                    if (name.size() && !path.ends_with('/')) path += "/";
                    mp[event->wd].push_back({event->mask, name, path + name});
                }
                {
                    MutexType::Lock lock(m_mutex);
                    for (auto& item: mp) {
                        auto it = m_cbs.find(item.first);
                        if (it != m_cbs.end()) {
                            auto callback = it->second.second;
                            go [callback, item]{
                                callback(item.second);
                            };
                        }
                    }
                }
            } while (true);
            return 0;
        };
    }

    return true;
}

bool FileMonitor::delWatch(const std::string& pathname) {
    MutexType::Lock lock(m_mutex);
    auto it = m_map.find(pathname);
    if (it == m_map.end()) return false;
    m_cbs.erase(it->second);
    inotify_rm_watch(m_inotify, it->second);
    m_map.erase(it);
    if(m_map.empty()) IOManager::GetThis()->cancelAllEvent(m_inotify);
    return true;
}

}