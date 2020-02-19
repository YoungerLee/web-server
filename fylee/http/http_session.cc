#include "http_session.h"
#include "http_parser.h"

namespace fylee {
namespace http {

HttpSession::HttpSession(Socket::ptr sock, bool owner)
    :SocketStream(sock, owner) {
}

HttpRequest::ptr HttpSession::recvRequest() {
    HttpRequestParser::ptr parser(new HttpRequestParser);
    uint64_t buff_size = HttpRequestParser::GetHttpRequestBufferSize();
    std::shared_ptr<char> buffer(
            new char[buff_size], [](char* ptr){
                delete[] ptr;
            });
    char* data = buffer.get();
    int offset = 0;
    do {
        int len = read(data + offset, buff_size - offset);
        if(len <= 0) {
            return nullptr;
        }
        len += offset;
        size_t nparse = parser->execute(data, len);
        if(parser->hasError()) {
            return nullptr;
        }
        offset = len - nparse;
        if(offset == (int)buff_size) {
            return nullptr;
        }
      
        if(parser->isFinished()) {
            break;
        }
    } while(true);
    int64_t length = parser->getContentLength();
    if(length > 0) {
        std::string body;
        body.resize(length);

        int len = 0;
        if(length >= offset) {
            memcpy(&body[0], data, offset);
            len = offset;
        } else {
            memcpy(&body[0], data, length);
            len = length;
        }
        length -= offset;
        if(length > 0) {
            if(readFixSize(&body[len], length) <= 0) {
                return nullptr;
            }
        }
        parser->getData()->setBody(body);
    }

    parser->getData()->init();
    return parser->getData();
}

int HttpSession::sendResponse(HttpResponse::ptr rsp) {
    std::stringstream ss;
    ss << *rsp;
    std::string data = ss.str();
    return writeFixSize(data.c_str(), data.size());
}

HttpRequest::ptr HttpSession::parseRequest(std::string msg) {
    HttpRequestParser::ptr parser(new HttpRequestParser);
    size_t len = msg.size();
    size_t s = parser->execute(&msg[0], len);
    msg.resize(msg.size() - s);
    if(parser->hasError() || !parser->isFinished()) {
        return nullptr;
    }
    int offset = len - s;
   
    int64_t length = parser->getContentLength();
    if(length > 0) {
        std::string body;
        body.resize(length);

        int len = 0;
        if(length >= offset) {
            memcpy(&body[0], msg.data(), offset);
            len = offset;
        } else {
            memcpy(&body[0], msg.data(), length);
            len = length;
        }
        length -= offset;
        if(length > 0) {
            if(readFixSize(&body[len], length) <= 0) {
                return nullptr;
            }
        }
        parser->getData()->setBody(body);
    }
    parser->getData()->init();
    return parser->getData();
}
}
}
