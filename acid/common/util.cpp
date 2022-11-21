//
// Created by zavier on 2021/10/20.
//
#include <sstream>
#include <execinfo.h>
#include "util.h"
namespace acid{
static std::shared_ptr<spdlog::logger> GetLogInstanceHelper() {
    auto instance = spdlog::stdout_color_mt("acid");
#if ACID_ENABLE_DEBUGGER
    instance->set_level(spdlog::level::debug);
    SPDLOG_LOGGER_INFO(instance, "ENABLE_DEBUGGER");
#endif
    return instance;
}

std::shared_ptr<spdlog::logger> GetLogInstance() {
    static std::shared_ptr<spdlog::logger> instance = GetLogInstanceHelper();
    return instance;
}

void Backtrace(std::vector<std::string>& bt, int size, int skip) {
    void** array = (void**)malloc((sizeof(void*) * size));
    size_t s = backtrace(array, size);

    char** strings = backtrace_symbols(array, s);
    if(strings == NULL) {
        SPDLOG_LOGGER_ERROR(GetLogInstance(),"backtrace_synbols error");
        free(array);
        return;
    }

    for(size_t i = skip; i < s; ++i) {
        bt.push_back(strings[i]);
    }

    free(strings);
    free(array);
}

std::string BacktraceToString(int size, int skip, const std::string& prefix) {
    std::vector<std::string> bt;
    Backtrace(bt, size, skip);
    std::stringstream ss;
    for(size_t i = 0; i < bt.size(); ++i) {
        ss << prefix << bt[i] << std::endl;
    }
    return ss.str();
}

uint64_t GetCurrentMS(){
    struct timeval tm;
    gettimeofday(&tm,0);
    return tm.tv_sec * 1000ul + tm.tv_usec / 1000;
}

uint64_t GetCurrentUS(){
    struct timeval tm;
    gettimeofday(&tm,0);
    return tm.tv_sec * 1000ul * 1000ul + tm.tv_usec;
}

}