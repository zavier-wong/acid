//
// Created by zavier on 2021/12/20.
//

#include "acid/log.h"
#include "acid/net/uri.h"

namespace acid {
static acid::Logger::ptr g_logger = ACID_LOG_ROOT();

Uri::Uri()
        : m_port(0)
        , m_cur(nullptr){

}

Uri::ptr Uri::Create(const std::string &uri) {
    if (uri.empty()) {
        return nullptr;
    }
    Uri::ptr res(new Uri);

    Task<bool> parse_task = res->parse();
    for (size_t i = 0; i < uri.size(); ++i) {
        res->m_cur = &uri[i];
        parse_task.resume();
        if (!parse_task.get()) {
            return nullptr;
        }
    }
    res->m_cur = nullptr;
    parse_task.resume();

    if (!parse_task.get()) {
        return nullptr;
    }
    return res;
}

Address::ptr Uri::createAddress() {
    IPAddress::ptr addr = Address::LookupAnyIPAddress(m_host);
    if (addr) {
        addr->setPort(getPort());
    }
    return addr;
}
const std::string& Uri::getPath() const {
    static std::string default_path = "/";
    if (m_scheme == "magnet") {
        return m_path;
    }
    return m_path.empty() ? default_path : m_path;
}

uint32_t Uri::getPort() const {
    if(m_port) {
        return m_port;
    }
    if(m_scheme == "http" || m_scheme == "ws") {
        return 80;
    } else if(m_scheme == "https" || m_scheme == "wss") {
        return 443;
    }
    return m_port;
}

/*
 foo://user@hostname:8080/over/there?name=ferret#nose
   \_/   \______________/\_________/ \_________/ \__/
    |           |            |            |        |
 scheme     authority       path        query   fragment
*/
std::ostream &Uri::dump(std::ostream &ostream) {
    ostream << m_scheme ;
    if (m_scheme.size()) {
        ostream << ':';
        if (m_scheme != "magnet") {
            ostream << "//";
        }
    }
    ostream << m_userinfo << (m_userinfo.empty()? "" : "@")
            << m_host
            << (isDefaultPort()? "" : ":" + std::to_string(m_port))
            << getPath()
            << (m_query.empty()? "" : "?") << m_query
            << (m_fragment.empty()? "" : "#") << m_fragment;
    return ostream;
}

std::string Uri::toString() {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

bool Uri::isDefaultPort() const {
    if(m_port == 0) {
        return true;
    }
    if(m_scheme == "http" || m_scheme == "ws") {
        return m_port == 80;
    } else if(m_scheme == "https" || m_scheme == "wss") {
        return m_port == 443;
    }
    return false;
}


/**
* 判断URI中的字符是否有效, O(1)时间复杂度
* @param p
* @return bool
*/
static inline bool IsValid(const char* p) {
    char c = *p;
    /// RFC3986文档规定，Url中允许包含英文字母（a-zA-Z）、数字（0-9）、-_.~4个特殊字符，这些字符被称为未保留字符
    ///  ! * ' ( ) ; : @ & = + $ , / ? # [ ]这些字符被称为保留字符

    switch (c) {
        case 'a'...'z':
        case 'A'...'Z':
        case '0'...'9':
        case '-':
        case '_':
        case '.':
        case '~':
        case '!':
        case '*':
        case '\'':
        case '(':
        case ')':
        case ';':
        case ':':
        case '@':
        case '&':
        case '=':
        case '+':
        case '$':
        case ',':
        case '/':
        case '?':
        case '#':
        case '[':
        case ']':
        case '%':
            return true;
        default:
            return false;
    }
}

#define yield \
co_yield true;  \
if(!m_cur) {    \
    co_return false;\
}

/**
* 常规uri格式
*      foo://user@hostname.com:8080/over/there?name=ferret#nose
*        \_/   \__________________/\_________/ \_________/ \__/
*        |           |                |            |        |
*     scheme     authority          path        query   fragment
* URI语法如下
* ([]表示可有可无)
* [ scheme : ] scheme-specific-part[ # fragment]
* 其中scheme-specific-part结构如下，其中//可以没有
*  [ //][ authority][ path][ ? query]
*  其中authority结构如下,注意：如果URI中存在authority字段，那么必有host字段
* [ user-info @] host [ : port]
* 所以URI结构如下
* [ scheme :][ user-info @] host [ : port][ path][ ? query][ # fragment]
*/
Task<bool> Uri::parse() {
    size_t port_idx = 0;
    std::string buff;
    while (*m_cur != ':' && *m_cur != '?' && *m_cur != '#' && *m_cur != '/' && IsValid(m_cur)) {
        buff.push_back(*m_cur);
        co_yield true;
        if (!m_cur) {
            m_host = std::move(buff);
            co_return true;
        }
    }
    if (*m_cur == '/') {
        /// 解析 host
        m_host = std::move(buff);
        goto parse_path;
    }
    yield;
    if (isdigit(*m_cur)) {
        /// 解析 host
        m_host = std::move(buff);
        goto parse_port;
    }

    m_scheme = std::move(buff);

    /// 解析 [ // ]
    if (*m_cur == '/'){
        yield;
        if (*m_cur != '/'){
            co_return false;
        }
        yield;
        /// 解析 authority, 注意：如果URI中存在authority字段，那么必有host字段
        /// [ user-info @] host [ : port]

        if (*m_cur != '/') {
            while (*m_cur != '@'  && *m_cur != '/' && IsValid(m_cur)) {
                buff.push_back(*m_cur);
                if (*m_cur == ':' && !port_idx) {
                    port_idx = buff.size();
                }
                co_yield true;
                if (!m_cur) {
                    if (port_idx) {
                        m_host = buff.substr(0, port_idx-1);
                        try {
                            m_port = std::stoul(buff.substr(port_idx));
                        } catch (...) {
                            co_return false;
                        }
                        buff.clear();
                    } else {
                        m_host = std::move(buff);
                    }
                    co_return true;
                }
            }
            if (buff.empty() || !IsValid(m_cur)) {
                co_return false;
            }

            /// 解析 userinfo
            if (*m_cur == '@') {
                port_idx = 0;
                m_userinfo = std::move(buff);
                yield;
                while (*m_cur != ':' && *m_cur != '/' && IsValid(m_cur)) {
                    buff.push_back(*m_cur);
                    co_yield true;
                    if (!m_cur) {
                        m_host = std::move(buff);
                        co_return true;
                    }
                }
                if (buff.empty() || !IsValid(m_cur)) {
                    co_return false;
                }

                /// 解析 host
                m_host = std::move(buff);
                /// 解析 port
                if (*m_cur == ':') {
                    yield;
                    parse_port:
                    while (isdigit(*m_cur)) {
                        buff.push_back(*m_cur);
                        co_yield true;
                        if (!m_cur) {
                            try {
                                m_port = std::stoul(buff);
                                co_return true;
                            } catch (...) {
                                co_return false;
                            }
                        }
                    }
                    if (*m_cur != '/' || buff.empty()) {
                        co_return false;
                    }
                    try {
                        m_port = std::stoul(buff);
                    } catch (...) {
                        co_return false;
                    }
                    buff.clear();
                }
            } else {
                if (port_idx) {
                    m_host = buff.substr(0, port_idx-1);
                    try {
                        m_port = std::stoul(buff.substr(port_idx));
                    } catch (...) {
                        co_return false;
                    }
                    buff.clear();
                } else {
                    m_host = std::move(buff);
                }

            }

        }
        /// 解析 path
        if (*m_cur == '/') {
            parse_path:
            buff.push_back('/');
            co_yield true;
            if (!m_cur) {
                m_path = std::move(buff);
                co_return true;
            }
            while (*m_cur != '?' && *m_cur != '#' && IsValid(m_cur)) {
                buff.push_back(*m_cur);
                co_yield true;
                if (!m_cur) {
                    m_path = std::move(buff);
                    co_return true;
                }
                if (!IsValid(m_cur)) {
                    co_return false;
                }
            }
            m_path = std::move(buff);
        }
    }

    /// 解析 query
    if (*m_cur == '?') {
        yield;
        while (*m_cur != '#' && IsValid(m_cur)) {
            buff.push_back(*m_cur);
            co_yield true;
            if (!m_cur) {
                m_query = std::move(buff);
                co_return true;
            }
            if (!IsValid(m_cur)) {
                co_return false;
            }
        }
        m_query = std::move(buff);
    }
    /// 解析 fragment
    if (*m_cur == '#') {
        yield;
        while (IsValid(m_cur)) {
            buff.push_back(*m_cur);
            co_yield true;
            if (!m_cur) {
                m_fragment = std::move(buff);
                co_return true;
            }
            if (!IsValid(m_cur)) {
                co_return false;
            }
        }
        m_fragment = std::move(buff);
    }
    co_return false;
}

}