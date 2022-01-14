//
// Created by zavier on 2021/12/15.
//

#ifndef ACID_HTTP_SESSION_H
#define ACID_HTTP_SESSION_H
#include <memory>
#include "acid/net/socket_stream.h"
#include "http.h"

namespace acid::http {
class HttpSession : public SocketStream {
public:
    using ptr = std::shared_ptr<HttpSession>;

    HttpSession(Socket::ptr socket, bool owner = true);

    HttpRequest::ptr recvRequest();

    ssize_t sendResponse(HttpResponse::ptr response);
};
}

#endif //ACID_HTTP_SESSION_H
