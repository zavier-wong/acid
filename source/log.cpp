//
// Created by zavier on 2021/10/25.
//

#include <cctype>
#include <map>
#include <iostream>
#include <stdarg.h>
#include <cstring>
#include "acid/config.h"
#include "acid/log.h"
#include "acid/config.h"

namespace acid{
static Logger::ptr g_logger = ACID_LOG_NAME("system");
const char* LogLevel::toString(LogLevel::Level level) {
    switch (level) {
        case Level::DEBUG:
            return "DEBUG";
        case Level::INFO:
            return "INFO";
        case Level::WARN:
            return "WARN";
        case Level::ERROR:
            return "ERROR";
        case Level::FATAL:
            return "FATAL";
        default:
            return "UNKNOW";
    }
    return "UNKNOW";
}

LogLevel::Level LogLevel::fromString(const std::string &str) {
    if(str == "DEBUG" || str == "debug")
        return LogLevel::DEBUG;
    else if(str == "INFO" || str == "info")
        return LogLevel::INFO;
    else if(str == "WARN" || str == "warn")
        return LogLevel::WARN;
    else if(str == "ERROR" || str == "error")
        return LogLevel::ERROR;
    else if(str == "FATAL" || str == "fatal")
        return LogLevel::FATAL;
    return LogLevel::UNKNOW;
}

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char *file, int32_t line,int32_t threadId,
                   int32_t fiberId, int32_t elapse, int64_t timestamp,const std::string& threadName)
        :m_logger(logger)
        ,m_level(level)
        ,m_file(file)
        ,m_line(line)
        ,m_threadId(threadId)
        ,m_fiberId(fiberId)
        ,m_elapse(elapse)
        ,m_time(timestamp)
        ,m_threadName(threadName){
    //const char* str=strrchr(file,'/');
    //m_file = str + 1;
}

void LogEvent::format(const char *fmt,...) {
    va_list va;
    va_start(va,fmt);
    char* ptr = nullptr;
    int len = vasprintf(&ptr,fmt,va);
    if(len >= 0){
        m_ss<<std::string(ptr);
        free(ptr);
    }
    va_end(va);
}

class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string& str="") {}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getContent();
    }
};

class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const std::string& str="") {}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << LogLevel::toString(level);
        //os << LogLevel::toString(event->getLevel());
    }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const std::string& str="") {}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getElapse();
    }
};

class LogNameFormatItem : public LogFormatter::FormatItem {
public:
    LogNameFormatItem(const std::string& str="") {}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getLogger()->getName();
    }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const std::string& str="") {}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getThreadId();
    }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
public:
    NewLineFormatItem(const std::string& str="") {}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << "\n";
    }
};
class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
    DateTimeFormatItem(const std::string& str="%Y-%m-%d %H:%M:%S") : m_str(str){
        if(m_str.empty()){
            m_str = "%Y-%m-%d %H:%M:%S";
        }
    }
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        struct tm tm;
        time_t time = event->getTime();
        localtime_r(&time, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), m_str.c_str(), &tm);
        os << buf;
    }
private:
    std::string m_str;
};

class FileNameFormatItem : public LogFormatter::FormatItem {
public:
    FileNameFormatItem(const std::string& str="") {}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getFile();
    }
};

class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const std::string& str="") {}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getLine();
    }
};

class TabLineFormatItem : public LogFormatter::FormatItem {
public:
    TabLineFormatItem(const std::string& str="") {}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << "\t";
    }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
public:
    FiberIdFormatItem(const std::string& str="") {}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getFiberId();
    }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
public:
    ThreadNameFormatItem(const std::string& str="") {}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << event->getThreadName();
    }
};

class StringFormatItem : public LogFormatter::FormatItem {
public:
    StringFormatItem(const std::string& str="")
        :m_str(str){}
    void format(std::ostream& os,std::shared_ptr<Logger> logger,LogLevel::Level level,LogEvent::ptr event) override {
        os << m_str;
    }
private:
    std::string m_str;
};

Logger::Logger(std::string  name)
        :m_name(std::move(name))
        ,m_level(LogLevel::DEBUG){
    m_formatter.reset(new LogFormatter());
}

void Logger::log(LogLevel::Level level,LogEvent::ptr event){
    if(level >= m_level){
        auto self = shared_from_this();
        MutexType::Lock lock(m_mutex);
        if(m_appenders.empty()){
            m_root->log(level,event);
        }else{
            for(auto& appender: m_appenders){
                appender->log(self,level,event);
            }
        }

    }
}

void Logger::debug(LogEvent::ptr event){
    log(LogLevel::DEBUG,event);
}

void Logger::info(LogEvent::ptr event){
    log(LogLevel::INFO,event);
}

void Logger::warn(LogEvent::ptr event){
    log(LogLevel::WARN,event);
}

void Logger::error(LogEvent::ptr event){
    log(LogLevel::ERROR,event);
}

void Logger::fatal(LogEvent::ptr event){
    log(LogLevel::FATAL,event);
}

void Logger::addAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    if(!appender->getFormatter()){
        MutexType::Lock  ll(appender->m_mutex);
        appender->m_formatter = m_formatter;
    }
    m_appenders.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    MutexType::Lock lock(m_mutex);
    for(auto it = m_appenders.begin(); it != m_appenders.end();it++){
        if(*it == appender){
            m_appenders.erase(it);
            return;
        }
    }
}

void Logger::clearAppender() {
    MutexType::Lock lock(m_mutex);
    m_appenders.clear();
}

void Logger::setFormatter(LogFormatter::ptr formatter) {
    MutexType::Lock lock(m_mutex);
    m_formatter = formatter;
    for(auto& i : m_appenders){
        MutexType::Lock ll(i->m_mutex);
        if(i->getHasFormatter() == false){
            i->m_formatter = formatter;
        }
    }
}

void Logger::setFormatter(const std::string &formatter) {
    LogFormatter::ptr new_val(new LogFormatter(formatter));
    if(new_val->getError()){
        std::cout << "Logger setFormatter name=" << m_name << " value="
            << formatter <<" invalid formatter" << std::endl;
        return;
    }
    setFormatter(new_val);
}

YAML::Node Logger::toYaml() {
    MutexType::Lock lock(m_mutex);
    YAML::Node res;
    res["name"] = m_name;
    res["level"] = LogLevel::toString(m_level);
    res["formatter"] = m_formatter->getPattern();
    for(auto& appender : m_appenders){
        res["appender"].push_back(appender->toYaml());
    }
    return res;
}

LogFormatter::ptr Logger::getFormatter() {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}

FileLogAppender::FileLogAppender(const std::string &filename)
    :m_filename(filename){
    reopen();
}

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    uint64_t now = time(0);
    if(now > m_lastTime + 3){
        reopen();
        m_lastTime = now;
    }
    MutexType::Lock lock(m_mutex);
    if(level >= m_level){
        m_formatter->format(m_filestream,logger,level,event);
    }
}

bool FileLogAppender::reopen() {
    MutexType::Lock lock(m_mutex);
    if(m_filestream){
        m_filestream.close();
    }
    m_filestream.open(m_filename);
    return !!m_filestream;
}

YAML::Node FileLogAppender::toYaml() {
    MutexType::Lock lock(m_mutex);
    YAML::Node res;
    res["type"] = "FileLogAppender";
    res["file"] = m_filename;
    res["level"] = LogLevel::toString(m_level);
    res["formatter"] = m_formatter->getPattern();
    return res;
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    if(level < m_level)
        return;
    const char *color = "";
    switch (level) {
        case LogLevel::UNKNOW:
            color = "";
            break;
        case LogLevel::DEBUG:
            color = "\033[34m";
            break;
        case LogLevel::INFO:
            //color = "\033[29m";
            break;
        case LogLevel::WARN:
            color = "\033[33m";
            break;
        case LogLevel::ERROR:
        case LogLevel::FATAL:
            color = "\033[31m";
            break;
    }
    MutexType::Lock lock(m_mutex);
    std::cout << color << m_formatter->format(logger,level,event) << "\033[0m" << std::flush;
}

YAML::Node StdoutLogAppender::toYaml() {
    MutexType::Lock lock(m_mutex);
    YAML::Node res;
    res["type"] = "StdoutLogAppender";
    res["level"] = LogLevel::toString(m_level);
    res["formatter"] = m_formatter->getPattern();
    return res;
}

LogFormatter::LogFormatter(const std::string &pattern)
    :m_pattern(pattern){
    init();
}

void LogFormatter::init() {
    /*
     *  %m 消息
     *  %p 日志级别
     *  %r 累计毫秒数
     *  %c 日志名称
     *  %t 线程id
     *  %n 换行
     *  %d 时间
     *  %f 文件名
     *  %l 行号
     *  %T 制表符
     *  %F 协程id
     *  %N 线程名称
     */
    static std::map<std::string ,std::function<FormatItem::ptr(const std::string&)>> itemMap{
            {"m", [] (const std::string& str) { return MessageFormatItem::ptr(new MessageFormatItem(str));}},
            {"p", [] (const std::string& str) { return LevelFormatItem::ptr(new LevelFormatItem(str));}},
            {"r", [] (const std::string& str) { return ElapseFormatItem::ptr(new ElapseFormatItem(str));}},
            {"c", [] (const std::string& str) { return LogNameFormatItem::ptr(new LogNameFormatItem(str));}},
            {"t", [] (const std::string& str) { return ThreadIdFormatItem::ptr(new ThreadIdFormatItem(str));}},
            {"n", [] (const std::string& str) { return NewLineFormatItem::ptr(new NewLineFormatItem(str));}},
            {"d", [] (const std::string& str) { return DateTimeFormatItem::ptr(new DateTimeFormatItem(str));}},
            {"f", [] (const std::string& str) { return FileNameFormatItem::ptr(new FileNameFormatItem(str));}},
            {"l", [] (const std::string& str) { return LineFormatItem::ptr(new LineFormatItem(str));}},
            {"T", [] (const std::string& str) { return TabLineFormatItem::ptr(new TabLineFormatItem(str));}},
            {"F", [] (const std::string& str) { return FiberIdFormatItem::ptr(new FiberIdFormatItem(str));}},
            {"N", [] (const std::string& str) { return ThreadNameFormatItem::ptr(new ThreadNameFormatItem(str));}},
    };

    std::string str;
    for(size_t i = 0; i < m_pattern.size(); i++){
        if(m_pattern[i] != '%'){
            str.push_back(m_pattern[i]);
            continue;
        }
        if(i + 1 >= m_pattern.size()){
            m_items.push_back(StringFormatItem::ptr(new StringFormatItem("<error format %>")));
            m_error = true;
            continue;
        }
        i++;
        if(m_pattern[i] == '%'){
            str.push_back('%');
            continue;
        }
        if(str.size()){
            m_items.push_back(StringFormatItem::ptr(new StringFormatItem(str)));
            str.clear();
        }
        if(m_pattern[i] != 'd'){
            std::string tmp(1,m_pattern[i]);
            auto it = itemMap.find(tmp);
            if(it == itemMap.end()){
                m_items.push_back(StringFormatItem::ptr(new StringFormatItem("<error format %"+tmp+">")));
                m_error = true;
            }else{
                m_items.push_back(it->second(tmp));
            }
        }else{
            //解析日期
            if(i + 1 >= m_pattern.size())
                continue;
            if(m_pattern[i+1] != '{'){
                m_items.push_back(itemMap["d"](""));
                continue;
            }
            i++;
            if(i + 1 >= m_pattern.size()){
                m_items.push_back(StringFormatItem::ptr(new StringFormatItem("<error format %d{ >")));
                m_error = true;
                continue;
            }
            i++;
            auto index = m_pattern.find_first_of('}',i);
            if(index == std::string::npos){
                m_items.push_back(StringFormatItem::ptr(new StringFormatItem("<error format %d{" + m_pattern.substr(i) + " >")));
                m_error = true;
                continue;
            }
            m_items.push_back(itemMap["d"](m_pattern.substr(i,index - i)));
            i = index;
        }
    }
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    std::stringstream ss;
    for(auto& item : m_items) {
        item->format(ss,logger,level,event);
    }
    return ss.str();
}

std::ostream & LogFormatter::format(std::ostream &os, std::shared_ptr <Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
    for(auto& item : m_items) {
        item->format(os,logger,level,event);
    }
    return os;
}

LogEventWrap::~LogEventWrap() {
    m_event->getLogger()->log(m_event->getLevel(),m_event);
}

struct LogAppenderDefine{
    enum AppenderType{
        NONE,
        STDOUTLOG,
        FILELOG
    };
    AppenderType type = NONE;
    LogLevel::Level level = LogLevel::UNKNOW;
    std::string file;
    std::string formatter;
    bool operator==(const LogAppenderDefine& oth) const{
        return type == oth.type
            && level == oth.level
            && file == oth.file
            && formatter == oth.formatter;
    }
};

struct LogDefine{
    std::string name;
    LogLevel::Level level = LogLevel::UNKNOW;
    std::string formatter;
    std::vector<LogAppenderDefine> appenders;
    bool operator==(const LogDefine& oth) const{
        return name == oth.name
            && level == oth.level
            && formatter == oth.formatter
            && appenders == oth.appenders;
    }
    bool operator<(const LogDefine& oth) const {
        return name < oth.name;
    }
};

template<>
class LaxicalCast<std::string ,LogAppenderDefine>{
public:
    LogAppenderDefine operator()(const std::string& str){
        YAML::Node node = YAML::Load(str);
        LogAppenderDefine res;
        std::string type = node["type"].IsDefined()? node["type"].as<std::string>(): "";
        std::transform(type.begin(),type.end(),type.begin(),::tolower);
        if(type == "stdoutlogappender"){
            res.type = LogAppenderDefine::STDOUTLOG;
        }else if(type == "filelogappender"){
            if(!node["file"].IsDefined()){
                res.type = LogAppenderDefine::NONE;
                std::cout << "Log config error: StdoutLogAppender file is null" << std::endl;
                return res;
            }
            res.file = node["file"].as<std::string>();
            res.type = LogAppenderDefine::FILELOG;
        }else{
            res.type = LogAppenderDefine::NONE;
            std::cout << "Log config error: appender type is null" << std::endl;
            return res;
        }

        res.level = LogLevel::fromString(node["level"].IsDefined()? node["level"].as<std::string>(): "");
        res.formatter = node["formatter"].IsDefined()? node["formatter"].as<std::string>(): "";
        return res;
    }
};

template<>
class LaxicalCast<LogAppenderDefine, std::string>{
public:
    std::string operator()(const LogAppenderDefine& log){
        YAML::Node node;
        node["level"] = LogLevel::toString(log.level);
        node["formatter"] = log.formatter;

        switch (log.type) {
            case LogAppenderDefine::NONE:
                node["type"] = "NONE";
                break;
            case LogAppenderDefine::STDOUTLOG:
                node["type"] = "StdoutLogAppender";
                break;
            case LogAppenderDefine::FILELOG:
                node["type"] = "FileLogAppender";
                node["file"] = log.file;
                break;
        }

        std::stringstream ss;
        ss<<node;
        return ss.str();
    }
};

template<>
class LaxicalCast<std::string ,std::set<LogDefine>>{
public:
    std::set<LogDefine> operator()(const std::string& str){
        YAML::Node node = YAML::Load(str);
        std::set<LogDefine> res;
        for (size_t i = 0; i < node.size(); ++i) {
            LogDefine ld;
            const YAML::Node& log = node[i];
            if (!log["name"].IsDefined()){
                std::cout << "Log config error: name is null" << str <<std::endl;
                continue;
            }
            ld.name = log["name"].as<std::string>();
            ld.level = LogLevel::fromString(log["level"].IsDefined()? log["level"].as<std::string>(): "");
            if(ld.level == LogLevel::UNKNOW){
                continue;
            }
            if(log["formatter"].IsDefined()){
                ld.formatter = log["formatter"].as<std::string>();
            }
            if(log["appender"].IsDefined()){
                std::vector<LogAppenderDefine> va;
                auto appender = log["appender"];
                std::stringstream ss;
                for(auto item:appender){
                    ss.str("");
                    ss<<item;
                    auto tmp = LaxicalCast<std::string,LogAppenderDefine>()(ss.str());
                    if(tmp.type == LogAppenderDefine::NONE){
                        continue;
                    }
                    va.push_back(tmp);
                }

                ld.appenders = std::move(va);
            }
            res.insert(ld);
        }
        return res;
    }
};

template<>
class LaxicalCast<LogDefine, std::string>{
public:
    std::string operator()(const LogDefine& log){
        YAML::Node node;
        node["name"] = log.name;
        node["level"] = LogLevel::toString(log.level);
        node["formatter"] = log.formatter;
        node["appender"] = YAML::Load(LaxicalCast<std::vector<LogAppenderDefine>,
                std::string>()(log.appenders));

        std::stringstream ss;
        ss<<node;
        return ss.str();
    }
};

ConfigVar<std::set<LogDefine>>::ptr g_log_defines =
        Config::Lookup("logs",std::set<LogDefine>(),"logs config");

struct LogIniter{
    LogIniter(){
        g_log_defines->addListener([](const std::set<LogDefine>& old_val,const std::set<LogDefine>& new_val){
            //新增或修改logger
            for(auto &i : new_val){

                Logger::ptr logger;
                logger = ACID_LOG_NAME(i.name);
                logger->setLevel(i.level);
                if(!i.formatter.empty()){
                    logger->setFormatter(i.formatter);
                }
                logger->clearAppender();
                for(auto& a : i.appenders){
                    LogAppender::ptr appender;
                    switch (a.type) {
                        case LogAppenderDefine::NONE:
                            // TO DO
                            continue;
                        case LogAppenderDefine::STDOUTLOG:
                            appender.reset(new StdoutLogAppender());
                            break;
                        case LogAppenderDefine::FILELOG:
                            appender.reset(new FileLogAppender(a.file));
                            break;
                    }
                    if(a.level == LogLevel::UNKNOW){
                        appender->setLevel(i.level);
                    } else{
                        appender->setLevel(a.level);
                    }

                    if(a.formatter.empty()){
                        appender->getFormatter() = logger->getFormatter();
                    }else{
                        appender->setFormatter(std::make_shared<LogFormatter>(a.formatter));
                    }
                    logger->addAppender(appender);
                }

            }
            //删除logger
            for(auto &i : old_val){
                auto it = new_val.find(i);
                if(it == new_val.end()){
                    auto logger = ACID_LOG_NAME(it->name);
                    logger->clearAppender();
                    logger->setLevel(LogLevel::UNKNOW);
                }
            }
        });
    }
};

static LogIniter __log_init;

LogManager::LogManager() {
    m_root.reset(new Logger());
    m_root->addAppender(std::make_shared<StdoutLogAppender>());
    m_loggers["root"]  = m_root;
}

void LogManager::init() {

}

Logger::ptr LogManager::getLogger(const std::string &name) {
    MutexType::Lock lock(m_mutex);
    auto it = m_loggers.find(name);
    if(it != m_loggers.end())
        return it->second;
    Logger::ptr logger = std::make_shared<Logger>(name);
    //logger->addAppender(std::make_shared<StdoutLogAppender>());
    logger->m_root = m_root;
    m_loggers[name] = logger;
    return logger;
}

void LogManager::setLogger(Logger::ptr logger) {
    MutexType::Lock lock(m_mutex);
    m_loggers[logger->getName()] = logger;
}

YAML::Node LogManager::toYaml() {
    MutexType::Lock lock(m_mutex);
    YAML::Node res;
    for(auto & logger : m_loggers){
        res["logs"].push_back(logger.second->toYaml());
    }
    return res;
}

std::string LogManager::toString() {
    MutexType::Lock lock(m_mutex);
    std::stringstream ss;
    ss<<toYaml();
    return ss.str();
}

void LogAppender::setFormatter(LogFormatter::ptr formatter) {
    MutexType::Lock lock(m_mutex);

    if(formatter->getError()){
        std::cout << "LogAppender setFormatter value="
                  << formatter->getPattern() <<" invalid formatter" << std::endl;
        return;
    }
    m_formatter = formatter;
    if(formatter){
        m_hasFormatter = true;
    }else{
        m_hasFormatter = false;
    }


}

LogFormatter::ptr LogAppender::getFormatter() {
    MutexType::Lock lock(m_mutex);
    return m_formatter;
}
} //namespace source