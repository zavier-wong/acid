//
// Created by zavier on 2022/1/13.
//

#include "acid/rpc/serializer.h"
#include "acid/rpc/rpc.h"
#include "acid/log.h"
#include "acid/config.h"
#include <iostream>
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();
void test1() {
    acid::rpc::Serializer s;
    acid::rpc::Result<int> r, t;
    r.setCode(acid::rpc::RPC_SUCCESS);
    r.setVal(23);
    s << r;
    s.reset();
    s >> t;
    ACID_LOG_DEBUG(g_logger) << t.toString();
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
    ACID_LOG_DEBUG(g_logger) << std::endl << acid::LaxicalCast<std::list<std::string>,std::string>()(b);
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
    ACID_LOG_DEBUG(g_logger) << std::endl << acid::LaxicalCast<std::vector<std::string>,std::string>()(b);
}

void test4() {
    std::set<std::string> a {"aa","bb"};
    acid::rpc::Serializer s;
    s << a;
    s.reset();
    std::set<std::string> b;
    s >> b;
    ACID_LOG_DEBUG(g_logger) << std::endl << acid::LaxicalCast<std::set<std::string>,std::string>()(b);
}

void test5() {
    std::map<int,std::vector<std::string>> a {{5,{"aa","bb"}},{7,{"aac","bbc"}}};
    acid::rpc::Serializer s;
    s << a;
    s.reset();
    std::map<int,std::vector<std::string>> b;
    s >> b;
    for(auto item:b){
        ACID_LOG_INFO(g_logger) << item.first << " " << item.second.size();//<< item.second;
        for(auto i:item.second) {
            ACID_LOG_INFO(g_logger) << i << " " ;
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
        ACID_LOG_INFO(g_logger) << item.first << " " << item.second;//<< item.second;
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
    ACID_LOG_DEBUG(g_logger) << std::endl << acid::LaxicalCast<std::list<std::string>,std::string>()(b);
}

void map2map() {

}

int main() {
    test6();
}