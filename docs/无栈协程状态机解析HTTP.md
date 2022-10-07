和普通的HTTP状态机一样，我们使用了一个主从状态机来解析。不同的是这里将从状态机改造成协程状态机，免去了繁琐的状态换，使解析流程的可读性、可维护性都大大提高。由于无栈协程的性能相当好，解析也有着不错的性能。

关于cpp20协程的使用教程已经很多了，这里不过多介绍语法了。但目前标准库缺少协程组件，先简单写一个`promise_type`，我们主要使用到了`co_return`和`co_yeild`，所以实现了`promise_type`的`return_value`和`yield_value`。

```cpp
template<class T>
struct Task {
    struct promise_type;
    using handle = std::coroutine_handle<promise_type>;
    struct promise_type {
        ...
        void return_value(T val) {
            m_val = val;
        }
        std::suspend_always yield_value(T val) {
            m_val = val;
            return {};
        }
        ...
        T m_val;
    };
    ...
    T get() {
        return m_handle.promise().m_val;
    }
    void resume() {
        m_handle.resume();
    }
    ...
private:
    handle m_handle = nullptr;
};
```


虽然从状态机使用协程消除了显式的状态，但主状态机还是需要状态转移，定义了以下的主状态
```cpp
/// 主状态机的状态
enum CheckState{
	NO_CHECK,		// 初始状态
	CHECK_LINE,		// 正在解析请求行
	CHECK_HEADER,	// 正在解析请求头
	CHECK_CHUNK		// 解析chunk
};
```
定义错误码
```cpp
 /// 错误码
enum Error {
	NO_ERROR = 0,		// 正常解析
	INVALID_METHOD,		// 无效的 Method
	INVALID_PATH,		// 无效的 Path 
	INVALID_VERSION,	// 无效的 Version
	INVALID_LINE,		// 无效的请求行
	INVALID_HEADER,		// 无效的请求头
	INVALID_CODE,		// 无效的 Code
	INVALID_REASON,		// 无效的 Reason
	INVALID_CHUNK		// 无效的 Chunk
};
```

由于Request和Response的格式一样都是请求行，请求头，正文，所以这两个主状态机一样，靠parse_line和parse_header两个虚函数多态来实现不同的从状态机。

```cpp
// 主状态机
size_t HttpParser::execute(char *data, size_t len, bool chunk) {
    ...
    size_t offset = 0;
    size_t i = 0;
    switch (m_checkState) {
        case NO_CHECK: {
            m_checkState = CHECK_LINE;
            m_parser = parse_line();
        }
            // 解析请求行
        case CHECK_LINE: {
			// 对请求行的每个字符解析
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
			// 对请求头的每个字符解析
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
        ...
    }
    memmove(data, data + offset, (len - offset));
    return offset;
}
```

这是解析的回调函数，结合下面的从状态机来看

```cpp
// Request回调
void on_request_method(const std::string& str);
void on_request_path(const std::string& str);
void on_request_query(const std::string& str);
void on_request_fragment(const std::string& str) ;
void on_request_version(const std::string& str);
void on_request_header(const std::string& key, const std::string& val);
void on_request_header_done();
// Response回调
void on_response_version(const std::string& str);
void on_response_status(const std::string& str);
void on_response_reason(const std::string& str);
void on_response_header(const std::string& key, const std::string& val);
void on_response_header_done();
void on_response_chunk(const std::string& str);
```

接下来就是协程状态机的主要部分，这个是解析Request的请求行的从状态机，可以看到解析的流程非常清晰，每解析完一部分就会触发对应的回调来获取请求内容。重点就在于parse_line()每次resume后都是在上一次解析过程的下一步，因为协程本身就是一个状态机，利用协程就消除了显式状态转移，使得解析过程连贯，逻辑清晰。

```cpp
/**
* @brief 解析 HTTP 请求的请求行
*/
Task<HttpParser::Error> HttpRequestParser::parse_line() {
    std::string buff;
    // 读取method
    while(isalpha(*m_cur)) {
        buff.push_back(*m_cur);
        co_yield NO_ERROR;
    }
    if(buff.empty()) {
        co_return INVALID_METHOD;
    }
    if(*m_cur != ' ') {
        // method之后不是空格，格式错误。
        co_return INVALID_METHOD;
    } else {
        // 读完method, 触发回调函数
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
```

解析Request的请求头

```cpp
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
```

解析Response的请求行

```cpp
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
```

解析Response的请求头

```cpp
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
        // 读取所有连续的字符存储入val中。
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
```

以上的解析基本都大同小异，只要理解了一个协程状态机，你就会发现任何的状态机基本都可以改写成协程形式的。

读完头部信息后，就可以通过ContentLength来读取正文的内容了，完整的协议接收如下。

```cpp
HttpRequest::ptr HttpSession::recvRequest() {
    HttpRequestParser::ptr parser(new HttpRequestParser);
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    std::string buffer;
    buffer.resize(buff_size);
    char* data = &buffer[0];
    size_t offset = 0;
    while (!parser->isFinished()) {
        ssize_t len = read(data + offset, buff_size - offset);
        if (len <= 0) {
            close();
            return nullptr;
        }
        len += offset;
        size_t nparse = parser->execute(data, len);
        if (parser->hasError() || nparse == 0) {
            ACID_LOG_DEBUG(g_logger) << "parser error code:" << parser->hasError();
            close();
            return nullptr;
        }
        offset = len - nparse;
    }
    uint64_t length = parser->getContentLength();
    if (length >= 0) {
        std::string body;
        body.resize(length);
        size_t len = 0;
        if (length >= offset) {
            memcpy(&body[0], data, offset);
            len = offset;
        } else {
            memcpy(&body[0], data, length);
            len = length;
        }
        length -= len;
        if(length > 0) {
            if(readFixSize(&body[len], length) <= 0) {
                close();
                return nullptr;
            }
        }
        parser->getData()->setBody(body);
    }
    return parser->getData();
}
```

## 最后

透过现象看本质，协程底层通过协程帧来保存了上下文，状态机也隐藏在了里面，使得我们不用显式定义状态，免去了繁琐的状态转换。如同大佬所说，协程之于状态机就像递归之于栈。

协程的使用场合不止异步，这是一个强大的利器，值得我们更深入去了解。

有兴趣可以来看看完整的代码 https://github.com/zavier-wong/acid/blob/main/source/http/parse.cpp