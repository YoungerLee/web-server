#ifndef __FYLEE_HTTP_PARSER_H__
#define __FYLEE_HTTP_PARSER_H__

#include "http.h"
#include "http11_parser.h"
#include "httpclient_parser.h"

namespace fylee {
namespace http {

class HttpRequestParser {
public:
    typedef std::shared_ptr<HttpRequestParser> ptr;

    HttpRequestParser();

    size_t execute(char* data, size_t len);

    int isFinished();

    int hasError(); 

    HttpRequest::ptr getData() const { return data_;}

    void setError(int v) { error_ = v;}

    uint64_t getContentLength();

    const http_parser& getParser() const { return parser_;}
public:

    static uint64_t GetHttpRequestBufferSize();

    static uint64_t GetHttpRequestMaxBodySize();
private:
    http_parser parser_;
    HttpRequest::ptr data_;
    int error_;
};

class HttpResponseParser {
public:
    typedef std::shared_ptr<HttpResponseParser> ptr;

    HttpResponseParser();

    size_t execute(char* data, size_t len, bool chunck);

    int isFinished();

    int hasError(); 

    HttpResponse::ptr getData() const { return data_;}

    void setError(int v) { error_ = v;}

    uint64_t getContentLength();

    const httpclient_parser& getParser() const { return parser_;}
public:
    static uint64_t GetHttpResponseBufferSize();

    static uint64_t GetHttpResponseMaxBodySize();
private:
    httpclient_parser parser_;
    HttpResponse::ptr data_;
    int error_;
};

}
}

#endif
