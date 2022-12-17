//
// Created by zavier on 2022/1/13.
//

#include "acid/rpc/serializer.h"
#include "acid/rpc/rpc.h"
#include "acid/common/config.h"
#include <iostream>

void test1() {
    acid::rpc::Serializer s;
    acid::rpc::Result<int> r, t;
    r.setCode(acid::rpc::RPC_SUCCESS);
    r.setVal(23);
    s << r;
    s.reset();
    s >> t;
    SPDLOG_INFO(t.toString());
}
void test2() {
    std::list<std::string> a;
    a.push_back("aa");
    a.push_back("bb");
    acid::rpc::Serializer s;
    s << a;
    s.reset();
    std::list<std::string> b;
    s >> b;
    SPDLOG_INFO(acid::LaxicalCast<std::list<std::string>,std::string>()(b));
}
void test3() {
    std::vector<std::string> a;
    a.push_back("aa");
    a.push_back("bb");
    acid::rpc::Serializer s;
    s << a;
    s.reset();
    std::vector<std::string> b;
    s >> b;
    SPDLOG_INFO(acid::LaxicalCast<std::vector<std::string>,std::string>()(b));
}

void test4() {
    std::set<std::string> a {"aa","bb"};
    acid::rpc::Serializer s;
    s << a;
    s.reset();
    std::set<std::string> b;
    s >> b;
    SPDLOG_INFO(acid::LaxicalCast<std::set<std::string>,std::string>()(b));
}

void test5() {
    std::map<int,std::vector<std::string>> a {{5,{"aa","bb"}},{7,{"aac","bbc"}}};
    acid::rpc::Serializer s;
    s << a;
    s.reset();
    std::map<int,std::vector<std::string>> b;
    s >> b;
    for(auto item:b){
        for(auto i:item.second) {
            SPDLOG_INFO("{} ", i);
        }
    }
}

template<typename T>
void test_map(T& a) {
    acid::rpc::Serializer s;
    s << a;
    s.reset();
    T b;
    s >> b;
    for(auto item:b){
        SPDLOG_INFO("{} {}", item.first, item.second);
    }
}

void test6() {
    std::map<int, std::string> a{{1,"a"},{2,"b"}};
    test_map(a);
}

void test7() {
    std::multimap<int, std::string> a{{1,"a"},{1,"a"}};
    test_map(a);
}

void test8() {
    std::unordered_multimap<int, std::string> a{{1,"a"},{1,"a"}};
    test_map(a);
}

void test9() {
    std::unordered_map<int, std::string> a{{1,"a"},{1,"a"}};
    test_map(a);
}

void seq2seq() {
    std::vector<std::string> a{"ab","cd"};
    acid::rpc::Serializer s;
    s << a;
    std::list<std::string> b;
    s.reset();
    s >> b;
    SPDLOG_INFO(acid::LaxicalCast<std::list<std::string>,std::string>()(b));
}

enum class Color : uint8_t {
    red,
    green,
    yellow,
};

struct UserDefine {
    int a{};
    char b{};
    std::string c{};
    Color d;
    std::vector<int> e;
    std::string toString() const {
        std::string str = fmt::format("a: {}, b: {}, c: {}, d: {}, e:[", a, b, c, static_cast<int>(d));
        for (auto item: e) {
            str += fmt::format("{} ", item);
        }
        return str + "]";
    }
};
// 无侵入式序列化
void test10() {
    UserDefine userDefine = UserDefine{.a = 1, .b = '2', .c = "3", .d = Color::green, .e = {4, 5}};
    SPDLOG_INFO(userDefine.toString());
    acid::rpc::Serializer serializer;
    serializer << userDefine;
    serializer.reset();

    userDefine = UserDefine{};
    SPDLOG_INFO(userDefine.toString());

    serializer >> userDefine;
    SPDLOG_INFO(userDefine.toString());
}

int main() {
    test10();
}