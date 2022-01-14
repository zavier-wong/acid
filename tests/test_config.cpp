//
// Created by zavier on 2021/10/26.
//
#include <list>
#include "acid/log.h"
#include "acid/config.h"
#include "yaml-cpp/yaml.h"
using namespace std;
auto port = acid::Config::Lookup("system.port",8080,"bind port");
auto vec = acid::Config::Lookup("system.vec",vector<int>{2,3},"vec");
//auto logss = acid::Config::Lockup("logs",vector<string>(),"logs");
class Person{
public:
    Person() = default;
    Person(const string& name,int age): m_age(age), m_name(name){}
    int m_age = 0;
    std::string m_name ;
    std::string toString() const {
        std::stringstream ss;
        ss<<"[ name="<<m_name<<", age="<<m_age<<" ]";
        return ss.str();
    }
    bool operator==(const Person& oth){
        return m_age == oth.m_age && m_name == oth.m_name;
    }
};
namespace acid{
template<>
class LaxicalCast<std::string ,Person>{
public:
    Person operator()(const std::string& str){
        YAML::Node node = YAML::Load(str);
        Person res;
        res.m_name = node["name"].as<std::string>();
        res.m_age = node["age"].as<int>();
        return res;
    }
};

template<>
class LaxicalCast<Person ,std::string >{
public:
    std::string operator()(const Person& v){
        YAML::Node node;
        node["name"] = v.m_name;
        node["age"] = v.m_age;
        std::stringstream ss;
        ss<<node;
        return ss.str();
    }
};
}
void test1(){
    ACID_LOG_DEBUG(ACID_LOG_ROOT())<<"Before \n"<<vec->toString();

    //YAML::Node config = YAML::LoadFile("config/log.yaml");
    acid::Config::LoadFromFile("config/log.yaml");
    acid::Config::Lookup("system.port","123"s,"vec");
    ACID_LOG_DEBUG(ACID_LOG_ROOT())<<"After \n"<<vec->toString();
    //cout<<acid::LaxicalCast<string,vector<int>>()("- 1\n- 2\n- 5")[2];
    //cout<<acid::LaxicalCast<list<list<vector<int>>>,string>()(list<list<vector<int>>>{{{1,2,4,5},{2,3}}});
    list<float> value{1.2,3.1415};
    cout<<acid::LaxicalCast<map<string,list<float>>,string>()(map<string,list<float>> {{"acid",value}});
}
void test2(){
    //auto pm = acid::Config::Lookup("map",map<string,vector<Person>>(),"");

    //pm->setValue(map<string,vector<Person>>());
    auto a = acid::Config::Lookup("class.person",Person(),"");
//    a->addListener(1,[](const auto& old_val, const auto& new_val){
//        ACID_LOG_INFO(ACID_LOG_ROOT())<<old_val.toString()<<" -> "<<new_val.toString();
//    });

    a->setValue(Person("aaa",44));
    //ACID_LOG_DEBUG(ACID_LOG_ROOT())<<"Before \n"<<a->toString()<<" "<<a->getValue().toString();
    //acid::Config::LoadFromFile("config/log.yaml");
    ACID_LOG_ERROR(ACID_LOG_ROOT())<<"After \n"<<a->toString()<<" "<<a->getValue().toString();;
    //auto tmp = pm->getValue();
    acid::Config::Visit([](acid::ConfigVarBase::ptr config){
        ACID_LOG_WARN(ACID_LOG_ROOT())<<config->getName()<<" "<<config->getDescription()<<" "<<config->toString();
    });

    //auto b = a->getValue().toString();
}
int main(){
    test2();
    //std::cout<<acid::LogMgr().GetInstance()->toYaml();
}
