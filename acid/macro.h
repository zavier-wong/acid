//
// Created by zavier on 2021/11/21.
//

#ifndef ACID_MACRO_H
#define ACID_MACRO_H

#include <assert.h>
#include <stdlib.h>
#include <string>
#include "util.h"

#if defined(__GNUC__)  || defined(__llvm__)
#   define ACID_LIKELY(x)       __builtin_expect(!!(x), 1)
#   define ACID_UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#   define ACID_LIKELY(x)       (x)
#   define ACID_UNLIKELY(x)     (x)
#endif

#define ACID_ASSERT(x) \
if(ACID_UNLIKELY(!(x))){          \
    ACID_LOG_ERROR(ACID_LOG_ROOT()) << "ASSERTION: " << #x \
        << "\nbacktrace:\n" << acid::BacktraceToString(100,2,"    "); \
    assert(x);         \
    exit(1);                       \
}
#define ACID_ASSERT2(x,m) \
if(ACID_UNLIKELY(!(x))){                 \
    ACID_LOG_ERROR(ACID_LOG_ROOT()) << "ASSERTION: " << #x \
        << "\n" << m <<"\n"              \
        << "\nbacktrace:\n" << acid::BacktraceToString(100,2,"    "); \
    assert(x);            \
    exit(1);                       \
}

#define ACID_STATIC_ASSERT(x) \
if constexpr(!(x)){          \
    ACID_LOG_ERROR(ACID_LOG_ROOT()) << "ASSERTION: " << #x \
        << "\nbacktrace:\n" << acid::BacktraceToString(100,2,"    "); \
    exit(1);                          \
}

#endif //ACID_MACRO_H
