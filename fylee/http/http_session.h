#ifndef __FYLEE_HTTP_SESSION_H__
#define __FYLEE_HTTP_SESSION_H__

#include "fylee/socket_stream.h"
#include "http.h"

namespace fylee {
namespace http {

class HttpSession : public SocketStream {
public:
    typedef std::shared_ptr<HttpSession> ptr;

    HttpSession(Socket::ptr sock, bool owner = true);

    HttpRequest::ptr recvRequest();

    HttpRequest::ptr parseRequest(std::string msg);

    int sendResponse(HttpResponse::ptr rsp);
};

}
}

#endif
