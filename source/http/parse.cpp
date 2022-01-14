//
// Created by zavier on 2021/12/13.
//

#include "acid/config.h"
#include "acid/http/parse.h"
#include "acid/log.h"
namespace acid::http {
static acid::Logger::ptr g_logger = ACID_LOG_NAME("system");

static acid::ConfigVar<uint64_t>::ptr g_http_request_buffer_size =
        acid::Config::Lookup("http.request.buffer_size"
                ,(uint64_t)(4 * 1024), "http request buffer size");

static acid::ConfigVar<uint64_t>::ptr g_http_request_max_body_size =
        acid::Config::Lookup("http.request.max_body_size"
                ,(uint64_t)(64 * 1024 * 1024), "http request max body size");

static acid::ConfigVar<uint64_t>::ptr g_http_response_buffer_size =
        acid::Config::Lookup("http.response.buffer_size"
                ,(uint64_t)(4 * 1024), "http response buffer size");

static acid::ConfigVar<uint64_t>::ptr g_http_response_max_body_size =
        acid::Config::Lookup("http.response.max_body_size"
                ,(uint64_t)(64 * 1024 * 1024), "http response max body size");

static uint64_t s_http_request_buffer_size = 0;
static uint64_t s_http_request_max_body_size = 0;
static uint64_t s_http_response_buffer_size = 0;
static uint64_t s_http_response_max_body_size = 0;

uint64_t HttpRequestParser::GetHttpRequestBufferSize() {
    return s_http_request_buffer_size;
}

uint64_t HttpRequestParser::GetHttpRequestMaxBodySize() {
    return s_http_request_max_body_size;
}

uint64_t HttpResponseParser::GetHttpResponseBufferSize() {
    return s_http_response_buffer_size;
}

uint64_t HttpResponseParser::GetHttpResponseMaxBodySize() {
    return s_http_response_max_body_size;
}

// 初始化解析的缓冲区大小
namespace {
    struct _RequestSizeIniter {
        _RequestSizeIniter() {
            s_http_request_buffer_size = g_http_request_buffer_size->getValue();
            s_http_request_max_body_size = g_http_request_max_body_size->getValue();
            s_http_response_buffer_size = g_http_response_buffer_size->getValue();
            s_http_response_max_body_size = g_http_response_max_body_size->getValue();

            g_http_request_buffer_size->addListener(
                    [](const uint64_t& ov, const uint64_t& nv){
                        s_http_request_buffer_size = nv;
                    });

            g_http_request_max_body_size->addListener(
                    [](const uint64_t& ov, const uint64_t& nv){
                        s_http_request_max_body_size = nv;
                    });

            g_http_response_buffer_size->addListener(
                    [](const uint64_t& ov, const uint64_t& nv){
                        s_http_response_buffer_size = nv;
                    });

            g_http_response_max_body_size->addListener(
                    [](const uint64_t& ov, const uint64_t& nv){
                        s_http_response_max_body_size = nv;
                    });
        }
    };
    static _RequestSizeIniter _init;
}


HttpParser::HttpParser() {

}

HttpParser::~HttpParser() {

}

int HttpParser::hasError() {
    return m_error;
}

int HttpParser::isFinished() {
    if(hasError()) {
        return -1;
    } else if (m_finish) {
        return 1;
    } else {
        return 0;
    }
}


size_t HttpParser::execute(char *data, size_t len, bool chunk) {
    if (chunk) {
        if (m_checkState != CHECK_CHUNK) {
            m_checkState = CHECK_CHUNK;
            m_finish = false;
            m_parser = parse_chunk();
        }
    }
    size_t offset = 0;
    size_t i = 0;
    switch (m_checkState) {
        case NO_CHECK: {
            m_checkState = CHECK_LINE;
            m_parser = parse_line();
        }
            // 解析请求行
        case CHECK_LINE: {
            for(; i < len; ++i){
                m_cur = data + i;
                m_parser.resume();
                m_error = m_parser.get();
                ++offset;
                if(isFinished()){
                    memmove(data, data + offset, (len - offset));
                    return offset;
                }
                if(m_checkState == CHECK_HEADER){
                    ++i;
                    m_parser = parse_header();
                    break;
                }
            }
            if(m_checkState != CHECK_HEADER) {
                memmove(data, data + offset, (len - offset));
                return offset;
            }
        }
        // 解析请求头
        case CHECK_HEADER: {
            for(; i < len; ++i){
                m_cur = data + i;
                m_parser.resume();
                m_error = m_parser.get();
                ++offset;
                if(isFinished()){
                    memmove(data, data + offset, (len - offset));
                    return offset;
                }
            }
            break;
        }
        case CHECK_CHUNK: {
            for(; i < len; ++i){
                m_cur = data + i;
                m_parser.resume();
                m_error = m_parser.get();
                ++offset;
                if(isFinished()){
                    memmove(data, data + offset, (len - offset));
                    return offset;
                }
            }
            break;
        }
    }
    memmove(data, data + offset, (len - offset));
    return offset;
}

uint64_t HttpRequestParser::getContentLength(){
    return m_data->getHeaderAs<uint64_t>("content-length", 0);
}

void HttpRequestParser::on_request_method(const std::string& str) {
    HttpMethod method = StringToMethod(str);
    if(method == HttpMethod::INVALID_METHOD) {
        ACID_LOG_WARN(g_logger) << "invalid http request method: " << str;
        setError(INVALID_METHOD);
        return;
    }
    m_data->setMethod(method);
}

void HttpRequestParser::on_request_path(const std::string& str) {
    m_data->setPath(std::move(str));
}

void HttpRequestParser::on_request_query(const std::string& str) {
    std::string key, val;
    size_t i, j;
    for (i = 0, j = 0; i < str.size(); ++i) {
        if(str[i] == '=') {
            key = str.substr(j, i - j);
            j = i + 1;
        } else if(str[i] == '&') {
            val = str.substr(j, i - j);
            j = i + 1;
            m_data->setParam(key, val);
            key.clear();
            val.clear();
        }
    }
    if(key.size()) {
        val = str.substr(j, i - j);
        if (val.size()) {
            m_data->setParam(key, val);
        }
    }
    m_data->setQuery(std::move(str));
}

void HttpRequestParser::on_request_fragment(const std::string& str) {
    m_data->setFragment(std::move(str));
}

void HttpRequestParser::on_request_version(const std::string& str) {
    uint8_t version = 0;
    if(str == "1.1") {
        version = 0x11;
    } else if(str == "1.0") {
        version = 0x10;
    } else {
        ACID_LOG_WARN(g_logger) << "invalid http request version: " << str;
        setError(INVALID_VERSION);
        return;
    }
    m_data->setVersion(version);
}

void HttpRequestParser::on_request_header(const std::string& key, const std::string& val) {
    if(key.empty()) {
        ACID_LOG_WARN(g_logger) << "invalid http request field length == 0";
        //setError(INVALID_HEADER);
        return;
    }
    m_data->setHeader(key, val);
}

void HttpRequestParser::on_request_header_done() {
    bool is_close = (m_data->getHeader("connection", "") != "keep-alive");
    m_data->setClose(is_close);
}

/**
* @brief 解析 HTTP 请求的请求行
*/
Task<HttpParser::Error> HttpRequestParser::parse_line() {
    std::string buff;
    // 读取method
    while(isalpha(*m_cur)) { // 读取所有连续的字符存储入method_中。
        buff.push_back(*m_cur);
        co_yield NO_ERROR;
    }
    // 读完了method
    if(buff.empty()) {
        co_return INVALID_METHOD;
    }
    // 读取空格
    if(*m_cur != ' ') {
        // method之后不是空格，格式错误。
        co_return INVALID_METHOD;
    } else {
        // 读完Method, 触发回调函数
        on_request_method(buff);
        if (m_error) {
            co_return INVALID_METHOD;
        }
        buff = "";
        co_yield NO_ERROR;
    }
    // 读取路径
    while(std::isprint(*m_cur) && strchr(" ?", *m_cur) == nullptr) {
        buff.push_back(*m_cur);
        co_yield NO_ERROR;
    }
    if(buff.empty()) {
        // path为空，格式错误
        co_return INVALID_PATH;
    }

    if(*m_cur == '?'){
        on_request_path(buff);
        buff = "";
        co_yield NO_ERROR;
        // 读取query
        while(std::isprint(*m_cur) && strchr(" #", *m_cur) == nullptr) {
            buff.push_back(*m_cur);
            co_yield NO_ERROR;
        }

        if(*m_cur == '#'){
            on_request_query(buff);
            buff = "";
            // 读取fragment
            while(std::isprint(*m_cur) && strchr(" ", *m_cur) == nullptr) {
                buff.push_back(*m_cur);
                co_yield NO_ERROR;
            }
            if(*m_cur != ' '){
                // fragment之后不是空格，格式错误。
                co_return INVALID_PATH;
            } else {
                on_request_fragment(buff);
                buff = "";
                co_yield NO_ERROR;
            }

        } else if(*m_cur != ' '){
            // query之后不是空格，格式错误。
            co_return INVALID_PATH;
        } else {
            on_request_query(buff);
            buff = "";
            co_yield NO_ERROR;
        }
    } else if(*m_cur != ' '){
        // path之后不是空格，格式错误。
        co_return INVALID_PATH;
    } else {
        on_request_path(buff);
        buff = "";
        co_yield NO_ERROR;
    }
    const char* version = "HTTP/1.";

    while (*version) {
        if(*m_cur != *version) {
            co_return INVALID_VERSION;
        }
        version++;
        co_yield NO_ERROR;
    }
    if(*m_cur != '1' && *m_cur != '0') {
        co_return INVALID_VERSION;
    } else {
        buff="1.";
        buff.push_back(*m_cur);
        on_request_version(buff);
        if (m_error) {
            co_return INVALID_VERSION;
        }
        buff = "";
        co_yield NO_ERROR;
    }
    if(*m_cur != '\r') {
        co_return INVALID_LINE;
    }
    co_yield NO_ERROR;
    if(*m_cur != '\n') {
        co_return INVALID_LINE;
    }
    // 状态转移
    m_checkState = CHECK_HEADER;
    co_return NO_ERROR;
}

/**
* @brief 解析 HTTP 请求的请求头
*/
Task<HttpParser::Error> HttpRequestParser::parse_header() {

    std::string key, val;
    // 循环读取header，直到读取到\r\n\r\n时完成
    while (!isFinished()){
        // 读取key
        while(std::isprint(*m_cur) && *m_cur != ':') { // 读取所有连续的字符存储入key中。
            key.push_back(*m_cur);
            co_yield NO_ERROR;
        }
        // 读取:
        if(*m_cur != ':') {
            co_return INVALID_HEADER;
        } else {
            co_yield NO_ERROR;
        }
        // 读取空格
        while(*m_cur == ' ') {
            co_yield NO_ERROR;
        }

        // 读取value
        //while(std::isalnum(*m_cur) || strchr("-/@=;.,: \'\"()?*", *m_cur) != nullptr) { // 读取所有连续的字符存储入val中。
        while (std::isprint(*m_cur)) {
            val.push_back(*m_cur);
            co_yield NO_ERROR;
        }
        if(*m_cur != '\r') {
            co_return INVALID_HEADER;
        }
        co_yield NO_ERROR;
        if(*m_cur != '\n') {
            co_return INVALID_HEADER;
        }
        co_yield NO_ERROR;
        if(*m_cur == '\r') {
            co_yield NO_ERROR;
            // 判断是不是 \r\n\r\n
            if(*m_cur == '\n'){
                on_request_header(key, val);
                on_request_header_done();
                m_finish = true;
                // 状态转移
                m_checkState = NO_CHECK;
                //yield;
                //会退出循环
            } else {
                co_return INVALID_HEADER;
            }
        } else {
            on_request_header(key, val);
            key.clear();
            val.clear();
            key.push_back(*m_cur);
            co_yield NO_ERROR;
        }
    }
    // 状态转移
    m_checkState = NO_CHECK;
    co_return NO_ERROR;
}

Task<HttpParser::Error> HttpRequestParser::parse_chunk() {
    /// 暂时不解析请求的 chunk
    co_return NO_ERROR;
}

uint64_t HttpResponseParser::getContentLength(){
    return m_data->getHeaderAs<uint64_t>("content-length", 0);
}
bool HttpResponseParser::isChunked() {
    return m_data->getHeader("Transfer-Encoding", "").size();
}
void HttpResponseParser::on_response_version(const std::string &str) {
    uint8_t v = 0;
    if(str == "1.1") {
        v = 0x11;
    } else if(str == "1.0") {
        v = 0x10;
    } else {
        ACID_LOG_WARN(g_logger) << "invalid http response version: "
                                << str;
        setError(INVALID_VERSION);
        return;
    }
    m_data->setVersion(v);
}

void HttpResponseParser::on_response_status(const std::string &str) {
    HttpStatus status = (HttpStatus)(std::stoi(str));
    m_data->setStatus(status);
}

void HttpResponseParser::on_response_reason(const std::string &str) {
    m_data->setReason(std::move(str));
}

void HttpResponseParser::on_response_header(const std::string &key, const std::string &val) {
    if(key.empty()) {
        ACID_LOG_WARN(g_logger) << "invalid http request field length == 0";
        //setError(INVALID_HEADER);
        return;
    }
    m_data->setHeader(key, val);
}

void HttpResponseParser::on_response_header_done() {
    bool is_close = (m_data->getHeader("connection", "") != "keep-alive");
    m_data->setClose(is_close);
}

void HttpResponseParser::on_response_chunk(const std::string &str) {
    m_data->setBody(str);
}

/**
* @brief 解析 HTTP 响应的请求行
*/
Task<HttpParser::Error> HttpResponseParser::parse_line() {
    std::string buff;
    const char* version = "HTTP/1.";
    // 判断HTTP版本是否合法
    while (*version) {
        if(*m_cur != *version) {
            co_return INVALID_VERSION;
        }
        version++;
        co_yield NO_ERROR;
    }

    if(*m_cur != '1' && *m_cur != '0') {
        co_return INVALID_VERSION;
    } else {
        buff="1.";
        buff.push_back(*m_cur);
        on_response_version(buff);
        if (m_error) {
            co_return INVALID_VERSION;
        }
        buff = "";
        co_yield NO_ERROR;
    }
    // 跳过空格
    while(*m_cur == ' ') {
        co_yield NO_ERROR;
    }
    // 读取status
    while(isdigit(*m_cur)) { // 读取所有连续的字符存储入method_中。
        buff.push_back(*m_cur);
        co_yield NO_ERROR;
    }
    // 读完了method
    if(buff.empty()) {
        co_return INVALID_CODE;
    }
    // 读取空格
    if(*m_cur != ' ') {
        // status之后不是空格，格式错误。
        co_return INVALID_CODE;
    } else {
        // 读完status, 触发回调函数
        on_response_status(buff);
        buff = "";
        co_yield NO_ERROR;
    }
    // 跳过空格
    while(*m_cur == ' ') {
        co_yield NO_ERROR;
    }
    // 读取Reason
    while(std::isalpha(*m_cur) || *m_cur == ' ') {
        buff.push_back(*m_cur);
        co_yield NO_ERROR;
    }
    if(buff.empty()) {
        // path为空，格式错误
        co_return INVALID_REASON;
    }

    if(*m_cur != '\r') {
        co_return INVALID_LINE;
    }
    co_yield NO_ERROR;
    if(*m_cur != '\n') {
        co_return INVALID_LINE;
    }
    on_response_reason(buff);
    // 状态转移
    m_checkState = CHECK_HEADER;
    co_return NO_ERROR;
}

/**
* @brief 解析 HTTP 响应的请求头
*/
Task<HttpParser::Error> HttpResponseParser::parse_header() {

    std::string key, val;
    // 循环读取header，直到读取到\r\n\r\n时完成
    while (!isFinished()){
        // 读取key
        while(std::isprint(*m_cur) && strchr(":", *m_cur) == nullptr) {
            key.push_back(*m_cur);
            co_yield NO_ERROR;
        }
        // 读取:
        if(*m_cur != ':') {
            co_return INVALID_HEADER;
        } else {
            co_yield NO_ERROR;
        }
        // 读取空格
        while(*m_cur == ' ') {
            co_yield NO_ERROR;
        }

        // 读取value
        //while(std::isalnum(*m_cur) || strchr("-/@=;.,: \'\"()?*", *m_cur) != nullptr) { // 读取所有连续的字符存储入val中。
        while (std::isprint(*m_cur)) {
            val.push_back(*m_cur);
            co_yield NO_ERROR;
        }
        if(*m_cur != '\r') {
            co_return INVALID_HEADER;
        }
        co_yield NO_ERROR;
        if(*m_cur != '\n') {
            co_return INVALID_HEADER;
        }
        co_yield NO_ERROR;
        if(*m_cur == '\r') {
            co_yield NO_ERROR;
            // 判断是不是 \r\n\r\n
            if(*m_cur == '\n'){
                on_response_header(key, val);
                on_response_header_done();
                m_finish = true;
                // 状态转移
                m_checkState = NO_CHECK;
                //co_yield NO_ERROR;
                //会退出循环
            } else {
                co_return INVALID_HEADER;
            }
        } else {
            on_response_header(key, val);
            key.clear();
            val.clear();
            key.push_back(*m_cur);
            co_yield NO_ERROR;
        }
    }
    // 状态转移
    m_checkState = NO_CHECK;
    co_return NO_ERROR;
}

/**
* @brief 解析 HTTP 响应的 chunk
*/
Task<HttpParser::Error> HttpResponseParser::parse_chunk() {
    std::string body;
    size_t len = 0;
    while (true){
        len = 0;
        // 获取 chunk 长度
        while (true) {
            if (std::isdigit(*m_cur)) {
                len = len * 16 + *m_cur - '0';
            } else if (*m_cur >= 'a' && *m_cur <= 'f') {
                len = len * 16 + *m_cur - 'a' + 10;
            } else if (*m_cur >= 'A' && *m_cur <= 'F') {
                len = len * 16 + *m_cur - 'A' + 10;
            }else {
                break;
            }
            co_yield NO_ERROR;
        }

        if (*m_cur != '\r') {
            co_return INVALID_CHUNK;
        }
        co_yield NO_ERROR;
        if (*m_cur != '\n') {
            co_return INVALID_CHUNK;
        }
        co_yield NO_ERROR;
        for (size_t i = 0; i < len; ++i) {
            body.push_back(*m_cur);
            co_yield NO_ERROR;
        }
        if (*m_cur != '\r') {
            co_return INVALID_CHUNK;
        }
        co_yield NO_ERROR;
        if (*m_cur != '\n') {
            co_return INVALID_CHUNK;
        }
        if (!len) {
            break;
        }
        co_yield NO_ERROR;
    }
    on_response_chunk(body);
    m_finish = true;
    co_return NO_ERROR;
}


}
