//
// Created by zavier on 2022/12/16.
//

#ifndef ACID_REFLECTION_H
#define ACID_REFLECTION_H

#include <variant>

namespace acid {
template <typename T>
constexpr std::size_t MembersCount = 0;

// 万能类型，可以隐式转换成任意类
struct UniversalType {
    template <typename T>
    operator T();
};

template <typename T, typename... Args>
consteval std::size_t MemberCountImpl() {
    if constexpr (requires {
        T {
            {Args{}}...,
            {UniversalType{}}
        };
    }) {
        return MemberCountImpl<T, Args..., UniversalType>();
    } else {
        return sizeof...(Args);
    }
}

template <typename T>
consteval std::size_t MemberCount() {
    // 说明用户已经特化该类的成员数量，直接返回
    if constexpr (MembersCount<T> > 0) {
        return MembersCount<T>;
    } else {
        return MemberCountImpl<T>();
    }
}

constexpr static auto MaxVisitMembers = 64;

constexpr decltype(auto)  VisitMembers(auto &&object, auto &&visitor) {
    using type = std::remove_cvref_t<decltype(object)>;
    constexpr auto Count = MemberCount<type>();
    if constexpr (Count == 0 && std::is_class_v<type> && !std::is_same_v<type, std::monostate>) {
        static_assert(!sizeof(type), "empty struct/class is not allowed!");
    }
    static_assert(Count <= MaxVisitMembers, "exceed max visit members");

    if constexpr (Count == 0) {
        return visitor();
    }
    else if constexpr (Count == 1) {
        auto &&[a1] = object;
        return visitor(a1);
    }
    else if constexpr (Count == 2) {
        auto &&[a1, a2] = object;
        return visitor(a1, a2);
    }
    else if constexpr (Count == 3) {
        auto &&[a1, a2, a3] = object;
        return visitor(a1, a2, a3);
    }
    else if constexpr (Count == 4) {
        auto &&[a1, a2, a3, a4] = object;
        return visitor(a1, a2, a3, a4);
    }
    else if constexpr (Count == 5) {
        auto &&[a1, a2, a3, a4, a5] = object;
        return visitor(a1, a2, a3, a4, a5);
    }
    else if constexpr (Count == 6) {
        auto &&[a1, a2, a3, a4, a5, a6] = object;
        return visitor(a1, a2, a3, a4, a5, a6);
    }
    else if constexpr (Count == 7) {
        auto &&[a1, a2, a3, a4, a5, a6, a7] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7);
    }
    else if constexpr (Count == 8) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8);
    }
    else if constexpr (Count == 9) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9);
    }
    else if constexpr (Count == 10) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
    }
    else if constexpr (Count == 11) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
    }
    else if constexpr (Count == 12) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
    }
    else if constexpr (Count == 13) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13);
    }
    else if constexpr (Count == 14) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14] =
        object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
    }
    else if constexpr (Count == 15) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15] =
        object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15);
    }
    else if constexpr (Count == 16) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16);
    }
    else if constexpr (Count == 17) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17);
    }
    else if constexpr (Count == 18) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18);
    }
    else if constexpr (Count == 19) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19);
    }
    else if constexpr (Count == 20) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20);
    }
    else if constexpr (Count == 21) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21);
    }
    else if constexpr (Count == 22) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22);
    }
    else if constexpr (Count == 23) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23);
    }
    else if constexpr (Count == 24) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24);
    }
    else if constexpr (Count == 25) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25);
    }
    else if constexpr (Count == 26) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26);
    }
    else if constexpr (Count == 27) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27] =
        object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27);
    }
    else if constexpr (Count == 28) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28] =
        object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28);
    }
    else if constexpr (Count == 29) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29);
    }
    else if constexpr (Count == 30) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30);
    }
    else if constexpr (Count == 31) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31);
    }
    else if constexpr (Count == 32) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32);
    }
    else if constexpr (Count == 33) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33);
    }
    else if constexpr (Count == 34) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34);
    }
    else if constexpr (Count == 35) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35);
    }
    else if constexpr (Count == 36) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36);
    }
    else if constexpr (Count == 37) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37);
    }
    else if constexpr (Count == 38) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38);
    }
    else if constexpr (Count == 39) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39);
    }
    else if constexpr (Count == 40) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40] =
        object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40);
    }
    else if constexpr (Count == 41) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41] =
        object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41);
    }
    else if constexpr (Count == 42) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42);
    }
    else if constexpr (Count == 43) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43);
    }
    else if constexpr (Count == 44) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44);
    }
    else if constexpr (Count == 45) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45);
    }
    else if constexpr (Count == 46) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46);
    }
    else if constexpr (Count == 47) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47);
    }
    else if constexpr (Count == 48) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48);
    }
    else if constexpr (Count == 49) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49);
    }
    else if constexpr (Count == 50) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50);
    }
    else if constexpr (Count == 51) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51);
    }
    else if constexpr (Count == 52) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52);
    }
    else if constexpr (Count == 53) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53] =
        object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53);
    }
    else if constexpr (Count == 54) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54] =
        object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53, a54);
    }
    else if constexpr (Count == 55) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54,
        a55] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53, a54, a55);
    }
    else if constexpr (Count == 56) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54,
        a55, a56] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53, a54, a55, a56);
    }
    else if constexpr (Count == 57) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54,
        a55, a56, a57] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53, a54, a55, a56, a57);
    }
    else if constexpr (Count == 58) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54,
        a55, a56, a57, a58] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53, a54, a55, a56, a57, a58);
    }
    else if constexpr (Count == 59) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54,
        a55, a56, a57, a58, a59] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53, a54, a55, a56, a57, a58, a59);
    }
    else if constexpr (Count == 60) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54,
        a55, a56, a57, a58, a59, a60] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53, a54, a55, a56, a57, a58, a59, a60);
    }
    else if constexpr (Count == 61) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54,
        a55, a56, a57, a58, a59, a60, a61] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53, a54, a55, a56, a57, a58, a59, a60, a61);
    }
    else if constexpr (Count == 62) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54,
        a55, a56, a57, a58, a59, a60, a61, a62] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53, a54, a55, a56, a57, a58, a59, a60, a61, a62);
    }
    else if constexpr (Count == 63) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54,
        a55, a56, a57, a58, a59, a60, a61, a62, a63] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53, a54, a55, a56, a57, a58, a59, a60, a61, a62,
                       a63);
    }
    else if constexpr (Count == 64) {
        auto &&[a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
        a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26, a27, a28,
        a29, a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, a40, a41,
        a42, a43, a44, a45, a46, a47, a48, a49, a50, a51, a52, a53, a54,
        a55, a56, a57, a58, a59, a60, a61, a62, a63, a64] = object;
        return visitor(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
                       a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25, a26,
                       a27, a28, a29, a30, a31, a32, a33, a34, a35, a36, a37, a38,
                       a39, a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, a50,
                       a51, a52, a53, a54, a55, a56, a57, a58, a59, a60, a61, a62,
                       a63, a64);
    }
}
}
#endif //ACID_REFLECTION_H
