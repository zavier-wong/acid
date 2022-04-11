//
// Created by zavier on 2022/4/11.
//

#ifndef ACID_FILEMONITOR_H
#define ACID_FILEMONITOR_H
#include <sys/inotify.h>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include "sync.h"

namespace acid {
/**
 * @brief 文件监听器，封装了 inotify
 * @details inotify是用来监视文件系统事件的机制，该机制可以用来监视文件和目录。
 * 监听文件有 bug ，建议监听目录
 */
class FileMonitor {
public:
    using MutexType = CoMutex;

    enum  {
        ALL = IN_ALL_EVENTS, // 监听全部事件
        ACCESS = IN_ACCESS, // 文件被访问
        ATTRIB = IN_ATTRIB, // 元数据被改变，例如权限、时间戳、扩展属性、链接数、UID、GID等
        CLOSE_WRITE = IN_CLOSE_WRITE, // 关闭打开写的文件
        CLOSE_NOWRITE = IN_CLOSE_NOWRITE, // 和IN_CLOSE_WRITE刚好相反，关闭不是打开写的文件
        CREATE = IN_CREATE, // 这个是用于目录，在监控的目录中创建目录或文件时发生
        DELETE = IN_DELETE, // 这个也是用于目录，在监控的目录中删除目录或文件时发生
        DELETE_SELF = IN_DELETE_SELF, // 监控的目录或文件本身被删除
        MODIFY = IN_MODIFY, // 文件被修改，这种事件会用到inotify_event中的cookie。
        MOVE_SELF = IN_MOVE_SELF, // 监控的文件或目录本身被移动
        MOVE = IN_MOVE, // 文件移动
        OPEN = IN_OPEN, // 文件被打开
    };

    struct EventContext {
        uint32_t event;     // 产生的事件
        std::string name;   // 产生此事件的文件名
        std::string path;   // 文件路径
    };

    FileMonitor();

    ~FileMonitor();

    bool addFileWatch(const std::string& pathname, uint32_t mask, std::function<void(EventContext)> cb) ;

    bool addDirectoryWatch(const std::string& pathname, uint32_t mask,
                           std::function<void(std::vector<EventContext>)> cb) ;

    bool addWatch(const std::string& pathname, uint32_t mask,
                  std::function<void(std::vector<EventContext>)> cb) ;

    bool delWatch(const std::string& pathname);

private:
    MutexType m_mutex;
    int m_inotify;
    std::map<std::string, int> m_map;
    std::map<int, std::pair<std::string,std::function<void(std::vector<EventContext>)>>> m_cbs;
};
}
#endif //ACID_FILEMONITOR_H
