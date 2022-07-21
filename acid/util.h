//
// Created by zavier on 2021/10/20.
//

#ifndef ACID_UTIL_H
#define ACID_UTIL_H

#include <byteswap.h>
#include <unistd.h>
#include <syscall.h>
#include <sys/time.h>

#include <bit>
#include <concepts>
#include <cstdint>
#include <string_view>
#include <algorithm>
#include <iostream>

#include "log.h"

namespace acid{

pid_t GetThreadId();
int64_t GetFiberId();

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

class defer_t final {
public:
    template <typename F>
    explicit defer_t(F&& f) noexcept : f_{std::forward<F>(f)} {
    }
    ~defer_t() {
        if (f_) {
            f_();
        }
    }
    defer_t(const defer_t&) = delete;
    defer_t& operator=(const defer_t&) = delete;
    defer_t(defer_t&& rhs) noexcept : f_{std::move(rhs.f_)} {}
    defer_t& operator=(defer_t&& rhs) noexcept {
        f_ = std::move(rhs.f_);
        return *this;
    }
private:
    std::function<void()> f_;
};

class defer_maker final {
public:
    template <typename F>
    defer_t operator+(F&& f) {
        return defer_t{std::forward<F>(f)};
    }
};

#define DEFER_CONCAT_NAME(l, r) l##r
#define DEFER_CREATE_NAME(l, r) DEFER_CONCAT_NAME(l, r)
#define defer auto DEFER_CREATE_NAME(defer_, __COUNTER__) = acid::defer_maker{} +
#define Defer defer [&]
#define Defer_ [&]
};

#endif //ACID_UTIL_H
