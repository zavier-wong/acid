//
// Created by zavier on 2021/10/20.
//

#ifndef ACID_UTIL_H
#define ACID_UTIL_H

#include <byteswap.h>
#include <unistd.h>
#include <syscall.h>
#include <ctime>

#include <bit>
#include <concepts>
#include <cstdint>
#include <string_view>
#include <algorithm>
#include <iostream>
#include <utility>
#if ACID_ENABLE_DEBUGGER
# define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// 前向声明
namespace co {
class Scheduler;
}

namespace acid{

std::shared_ptr<spdlog::logger> GetLogInstance();

void Backtrace(std::vector<std::string>& bt, int size, int skip = 1 );
std::string BacktraceToString(int size, int skip = 2, const std::string& prefix = "");

//时间
uint64_t GetCurrentMS();
uint64_t GetCurrentUS();


//bit

/**
* @brief 字节序转换
*/
template<std::integral T>
constexpr T ByteSwap(T value) {
    if constexpr(sizeof(T) == sizeof(uint8_t)){
        return (T)bswap_16((uint16_t)value);
    } else if constexpr(sizeof(T) == sizeof(uint16_t)){
        return (T)bswap_16((uint16_t)value);
    } else if constexpr(sizeof(T) == sizeof(uint32_t)){
        return (T)bswap_32((uint32_t)value);
    } else if constexpr(sizeof(T) == sizeof(uint64_t)){
        return (T)bswap_64((uint64_t)value);
    }
}

/**
* @brief 网络序主机序转换
*/
template<std::integral T>
constexpr T EndianCast(T value) {
    if constexpr(sizeof(T) == sizeof(uint8_t)
                 || std::endian::native == std::endian::big){
        return value;
    } else if constexpr(std::endian::native == std::endian::little){
        return ByteSwap(value);
    }
}

class CycleTimerTocken {
public:
    CycleTimerTocken(std::shared_ptr<bool> cancel = nullptr) : m_cancel(std::move(cancel)) {}

    operator bool () {
        return m_cancel || !isCancel();
    }

    void stop() {
        if (!m_cancel) return;
        *m_cancel = true;
    }

    bool isCancel() {
        if (!m_cancel) {
            return true;
        }
        return *m_cancel;
    }

private:
    std::shared_ptr<bool> m_cancel;
};

/**
 * 设置循环定时器
 * @param interval_ms 循环间隔（ms）
 * @param cb 回调函数
 * @param worker 定时器所在的调度器，如果默认则会先检查函数是否在协程内，在的话则设置为所在协程的调度器，否则设置为 co_sched
 * @param times 循环次数，-1为无限循环
 * @return CycleTimerTocken 可用来停止循环定时器
 */
CycleTimerTocken CycleTimer(unsigned interval_ms, std::function<void()> cb, co::Scheduler* worker = nullptr, int times = -1);

};

#endif //ACID_UTIL_H
