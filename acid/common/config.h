//
// Created by zavier on 2021/10/26.
//

#ifndef ACID_CONFIG_H
#define ACID_CONFIG_H

#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <set>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <libgo/libgo.h>
#include <yaml-cpp/yaml.h>
#include "lexical_cast.h"
#include "util.h"

namespace acid {

class ConfigVarBase{
public:
    using ptr = std::shared_ptr<ConfigVarBase>;
    ConfigVarBase(std::string name,std::string description) : m_name(std::move(name)),m_description(std::move(description)) {}
    virtual ~ConfigVarBase() = default;
    std::string getName() const { return m_name;}
    std::string getDescription() const { return m_description;}

    virtual std::string getTypeName() const = 0;
    virtual std::string toString() = 0;
    virtual bool fromString(const std::string& str) = 0;
protected:
    std::string m_name;
    std::string m_description;
};

template<class From,class To>
class LaxicalCast{
public:
    To operator()(const From& from){
        return lexical_cast<To>(from);
    }
};

template<class T>
class LaxicalCast<std::string ,std::vector<T>>{
public:
    std::vector<T> operator()(const std::string& str){
        YAML::Node node = YAML::Load(str);
        std::vector<T> res;
        std::stringstream ss;
        for(auto item:node){
            ss.str("");
            ss<<item;
            res.push_back(LaxicalCast<std::string,T>()(ss.str()));
        }
        return res;
    }
};

template<class T>
class LaxicalCast<std::vector<T> ,std::string >{
public:
    std::string operator()(const std::vector<T>& v){
        YAML::Node node;
        for(auto& item:v){
            //node.push_back(item);
            node.push_back(YAML::Load(LaxicalCast<T,std::string>()(item)));
        }
        std::stringstream ss;
        ss<<node;
        return ss.str();
    }
};


template<class T>
class LaxicalCast<std::string ,std::list<T>>{
public:
    std::list<T> operator()(const std::string& str){
        YAML::Node node = YAML::Load(str);
        std::list<T> res;
        std::stringstream ss;
        for(auto item:node){
            ss.str("");
            ss<<item;
            res.push_back(LaxicalCast<std::string,T>()(ss.str()));
        }
        return res;
    }
};

template<class T>
class LaxicalCast<std::list<T> ,std::string >{
public:
    std::string operator()(const std::list<T>& v){
        YAML::Node node;
        for(auto& item:v){
            node.push_back(YAML::Load(LaxicalCast<T,std::string>()(item)));
        }
        std::stringstream ss;
        ss<<node;
        return ss.str();
    }
};


template<class T>
class LaxicalCast<std::string ,std::set<T>>{
public:
    std::set<T> operator()(const std::string& str){
        YAML::Node node = YAML::Load(str);
        std::set<T> res;
        std::stringstream ss;
        for(auto item:node){
            ss.str("");
            ss<<item;
            res.template emplace(LaxicalCast<std::string,T>()(ss.str()));
        }
        return res;
    }
};

template<class T>
class LaxicalCast<std::set<T> ,std::string >{
public:
    std::string operator()(const std::set<T>& v){
        YAML::Node node;
        for(auto& item:v){
            node.push_back(YAML::Load(LaxicalCast<T,std::string>()(item)));
        }
        std::stringstream ss;
        ss<<node;
        return ss.str();
    }
};

template<class T>
class LaxicalCast<std::string ,std::unordered_set<T>>{
public:
    std::unordered_set<T> operator()(const std::string& str){
        YAML::Node node = YAML::Load(str);
        std::unordered_set<T> res;
        std::stringstream ss;
        for(auto item:node){
            ss.str("");
            ss<<item;
            res.push_back(LaxicalCast<std::string,T>()(ss.str()));
        }
        return res;
    }
};

template<class T>
class LaxicalCast<std::unordered_set<T> ,std::string >{
public:
    std::string operator()(const std::unordered_set<T>& v){
        YAML::Node node;
        for(auto& item:v){
            node.push_back(YAML::Load(LaxicalCast<T,std::string>()(item)));
        }
        std::stringstream ss;
        ss<<node;
        return ss.str();
    }
};

template<class T>
class LaxicalCast<std::string ,std::map<std::string,T>>{
public:
    std::map<std::string,T> operator()(const std::string& str){
        YAML::Node node = YAML::Load(str);
        std::map<std::string,T> res;
        std::stringstream ss;
        for(auto item:node){
            ss.str("");
            ss<<item.second;
            T value = LaxicalCast<std::string ,T>()(ss.str());
            res.template emplace(item.first.Scalar(),value);
        }
        return res;
    }
};

template<class T>
class LaxicalCast<std::map<std::string,T>,std::string >{
public:
    std::string operator()(const std::map<std::string,T>& v){
        YAML::Node node;
        for(auto& item:v){
            node[item.first] = YAML::Load(LaxicalCast<T,std::string>()(item.second));
        }
        std::stringstream ss;
        ss<<node;
        return ss.str();
    }
};


template<class T>
class LaxicalCast<std::string ,std::unordered_map<std::string,T>>{
public:
    std::unordered_map<std::string,T> operator()(const std::string& str){
        YAML::Node node = YAML::Load(str);
        std::unordered_map<std::string,T> res;
        std::stringstream ss;
        for(auto item:node){
            ss.str("");
            ss<<item.second;
            T value = LaxicalCast<std::string ,T>()(ss.str());
            res.template emplace(item.first.Scalar(),value);
        }
        return res;
    }
};

template<class T>
class LaxicalCast<std::unordered_map<std::string,T>,std::string >{
public:
    std::string operator()(const std::unordered_map<std::string,T>& v){
        YAML::Node node;
        for(auto& item:v){
            node[item.first] = YAML::Load(LaxicalCast<T,std::string>()(item.second));
        }
        std::stringstream ss;
        ss<<node;
        return ss.str();
    }
};

//FromStr T operator()(const std::string&)
//ToStr  std::string operator()(const T&)
template<class T, class FromStr = LaxicalCast<std::string, T>
                , class ToStr = LaxicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase{
public:
    using ptr = std::shared_ptr<ConfigVar>;
    //配置事件变更回调函数
    using Callback = std::function<void(const T &,const T &)>;

    ConfigVar(const std::string& name, const T& value, const std::string description = "")
    : ConfigVarBase(name, description)
    ,m_val(value){
    }
    std::string toString() override {
        try {
            std::unique_lock<co_rmutex> lock(m_mutex.Reader());
            return ToStr()(m_val);
        }catch (std::exception& e){
            GetLogInstance()->template
                    error("ConfigVar::toString() exception {} convert: {} toString", e.what(), typeid(T).name());
        }
        return "";
    }
    bool fromString(const std::string& str) override {
        try {
            setValue(FromStr()(str));
            return true;
        }catch (std::exception& e){
            GetLogInstance()->template
                    error("ConfigVar::fromString() exception {} convert: String to {}", e.what(), typeid(T).name());
        }
        return false;
    }
    const T getValue() {
        std::unique_lock<co_rmutex> lock(m_mutex.Reader());
        return m_val;
    }
    void setValue(const T& value) {
        {
            std::unique_lock<co_rmutex> lock(m_mutex.Reader());
            if(value == m_val){
                return;
            }
            for(auto& i:m_callbacks){
                i.second(m_val,value);
            }
        }
        std::unique_lock<co_wmutex> lock(m_mutex.Writer());
        m_val = value;
    }
    std::string getTypeName() const override { return typeid(T).name();}

    uint64_t addListener(Callback cb){
        static uint64_t s_fun_id = 0;
        std::unique_lock<co_wmutex> lock(m_mutex.Writer());
        s_fun_id++;
        m_callbacks[s_fun_id] = cb;
        return s_fun_id;
    }
    void delListener(int64_t key){
        std::unique_lock<co_wmutex> lock(m_mutex.Writer());
        m_callbacks.erase(key);
    }
    void clearListener(){
        std::unique_lock<co_wmutex> lock(m_mutex.Writer());
        m_callbacks.clear();
    }
    Callback getListener(int64_t key){
        std::unique_lock<co_rmutex> lock(m_mutex.Reader());
        auto it = m_callbacks.template find(key);
        if(it == m_callbacks.end())
            return nullptr;
        else
            return it->second;
    }
private:
    T m_val;
    //变更回调函数集合，key值可用hash
    std::map<int64_t , Callback > m_callbacks;
    co_rwmutex m_mutex;
};

class Config{
public:
    using ConfigVarMap = std::map<std::string, ConfigVarBase::ptr>;
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name){
        std::unique_lock<co_rmutex> lock(GetMutex().Reader());
        auto it = GetDatas().find(name);
        if(it == GetDatas().end()){
            return nullptr;
        }
        return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
    }
    template<class T>
    static typename ConfigVar<T>::ptr Lookup(const std::string& name,const T& value,const std::string& description){
        std::unique_lock<co_wmutex> lock(GetMutex().Writer());
        auto it = GetDatas().find(name);
        if(it != GetDatas().end()){
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
            if(tmp){
                SPDLOG_LOGGER_INFO(GetLogInstance(), "lookup name={} already exist", name);
                return tmp;
            }else{
                SPDLOG_LOGGER_INFO(GetLogInstance(), "lookup name={} already exist but type not {}, real type is {}, value is {}",
                                   name, typeid(T).name() ,it->second->getTypeName(), it->second->toString());
                return nullptr;
            }
        }
        if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._")
                != std::string::npos){
            SPDLOG_LOGGER_ERROR(GetLogInstance(), "lookup invalid name {}", name);
            throw std::invalid_argument(name);
        }
        auto v = std::make_shared<ConfigVar<T>>(name,value,description);
        GetDatas()[name] = v;
        return  v;
    }
    static ConfigVarBase::ptr LookupBase(const std::string& name);
    static void LoadFromFile(const std::string& file);
    static void LoadFromYaml(const YAML::Node& root);
    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);
private:

    static ConfigVarMap& GetDatas(){
        static ConfigVarMap s_datas;
        return s_datas;
    }

    static co_rwmutex& GetMutex(){
        static co_rwmutex s_mutex;
        return s_mutex;
    }
};

}

#endif //ACID_CONFIG_H
