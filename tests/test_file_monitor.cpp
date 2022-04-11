//
// Created by zavier on 2022/4/11.
//
#include "acid/log.h"
#include "acid/file_monitor.h"
using namespace acid;
int main() {

    FileMonitor fileMonitor;
    fileMonitor.addFileWatch("/home/zavier/log", FileMonitor::ALL,
                             [](FileMonitor::EventContext ctx){
        LOG_DEBUG << "log file modify";
    });

    fileMonitor.addDirectoryWatch("/home/zavier/tmp", FileMonitor::ALL,
                              [](std::vector<FileMonitor::EventContext> vec){
        for(auto e: vec) {
          LOG_DEBUG << e.path;
        }
    });

    sleep(100);

}