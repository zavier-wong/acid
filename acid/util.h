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

}

#endif //ACID_UTIL_H
