//
// Created by zavier on 2021/12/27.
//
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>

#include "file_servlet.h"

namespace acid::http {
FileServlet::FileServlet(const std::string &path)
    : Servlet("FileServlet"), m_path(path) {

}

int32_t FileServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
    if (request->getPath().find("..") != std::string::npos) {
        NotFoundServlet("acid").handle(request, response, session);
        return 1;
    }
    std::string filename = m_path + request->getPath();

    struct stat filestat;
    if(stat(filename.c_str(), &filestat) < 0){
        NotFoundServlet("acid").handle(request, response, session);
        return 1;
    }

    if(!S_ISREG(filestat.st_mode) || !(S_IRUSR & filestat.st_mode)){
        NotFoundServlet("acid").handle(request, response, session);
        return 1;
    }

    response->setStatus(http::HttpStatus::OK);
    //response->setHeader("Content-Type","text/plain");
    response->setHeader("Server", "acid/1.0.0");
    response->setHeader("Content-length",std::to_string(filestat.st_size));

    session->sendResponse(response);

    int filefd = open(filename.c_str(), O_RDONLY);
    sendfile(session->getSocket()->getSocket(), filefd, 0, filestat.st_size);

    return 1;
}

}