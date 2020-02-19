#include "http.h"
#include "fylee/util.h"

namespace fylee {
namespace http {

HttpMethod StringToHttpMethod(const std::string& m) {
#define XX(num, name, string) \
    if(strcmp(#string, m.c_str()) == 0) { \
        return HttpMethod::name; \
    }
    HTTP_METHOD_MAP(XX);
#undef XX
    return HttpMethod::INVALID_METHOD;
}

HttpMethod CharsToHttpMethod(const char* m) {
#define XX(num, name, string) \
    if(strncmp(#string, m, strlen(#string)) == 0) { \
        return HttpMethod::name; \
    }
    HTTP_METHOD_MAP(XX);
#undef XX
    return HttpMethod::INVALID_METHOD;
}

static const char* s_method_string[] = {
#define XX(num, name, string) #string,
    HTTP_METHOD_MAP(XX)
#undef XX
};

const char* HttpMethodToString(const HttpMethod& m) {
    uint32_t idx = (uint32_t)m;
    if(idx >= (sizeof(s_method_string) / sizeof(s_method_string[0]))) {
        return "<unknown>";
    }
    return s_method_string[idx];
}

const char* HttpStatusToString(const HttpStatus& s) {
    switch(s) {
#define XX(code, name, msg) \
        case HttpStatus::name: \
            return #msg;
        HTTP_STATUS_MAP(XX);
#undef XX
        default:
            return "<unknown>";
    }
}

bool CaseInsensitiveLess::operator()(const std::string& lhs
                            ,const std::string& rhs) const {
    return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
}

HttpRequest::HttpRequest(uint8_t version, bool close)
    :method_(HttpMethod::GET)
    ,version_(version)
    ,close_(close)
    ,websocket_(false)
    ,parserParamFlag_(0)
    ,path_("/") {
}

std::string HttpRequest::getHeader(const std::string& key
                            ,const std::string& def) const {
    auto it = headers_.find(key);
    return it == headers_.end() ? def : it->second;
}

std::shared_ptr<HttpResponse> HttpRequest::createResponse() {
    HttpResponse::ptr rsp(new HttpResponse(getVersion()
                            ,isClose()));
    return rsp;
}

std::string HttpRequest::getParam(const std::string& key
                            ,const std::string& def) {
    initQueryParam();
    initBodyParam();
    auto it = params_.find(key);
    return it == params_.end() ? def : it->second;
}

std::string HttpRequest::getCookie(const std::string& key
                            ,const std::string& def) {
    initCookies();
    auto it = cookies_.find(key);
    return it == cookies_.end() ? def : it->second;
}

void HttpRequest::setHeader(const std::string& key, const std::string& val) {
    headers_[key] = val;
}

void HttpRequest::setParam(const std::string& key, const std::string& val) {
    params_[key] = val;
}

void HttpRequest::setCookie(const std::string& key, const std::string& val) {
    cookies_[key] = val;
}

void HttpRequest::delHeader(const std::string& key) {
    headers_.erase(key);
}

void HttpRequest::delParam(const std::string& key) {
    params_.erase(key);
}

void HttpRequest::delCookie(const std::string& key) {
    cookies_.erase(key);
}

bool HttpRequest::hasHeader(const std::string& key, std::string* val) {
    auto it = headers_.find(key);
    if(it == headers_.end()) {
        return false;
    }
    if(val) {
        *val = it->second;
    }
    return true;
}

bool HttpRequest::hasParam(const std::string& key, std::string* val) {
    initQueryParam();
    initBodyParam();
    auto it = params_.find(key);
    if(it == params_.end()) {
        return false;
    }
    if(val) {
        *val = it->second;
    }
    return true;
}

bool HttpRequest::hasCookie(const std::string& key, std::string* val) {
    initCookies();
    auto it = cookies_.find(key);
    if(it == cookies_.end()) {
        return false;
    }
    if(val) {
        *val = it->second;
    }
    return true;
}

std::string HttpRequest::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

std::ostream& HttpRequest::dump(std::ostream& os) const {
    //GET /uri HTTP/1.1
    //Host: wwww.sylar.top
    //
    //
    os << HttpMethodToString(method_) << " "
       << path_
       << (query_.empty() ? "" : "?")
       << query_
       << (fragment_.empty() ? "" : "#")
       << fragment_
       << " HTTP/"
       << ((uint32_t)(version_ >> 4))
       << "."
       << ((uint32_t)(version_ & 0x0F))
       << "\r\n";
    if(!websocket_) {
        os << "connection: " << (close_ ? "close" : "keep-alive") << "\r\n";
    }
    for(auto& i : headers_) {
        if(!websocket_ && strcasecmp(i.first.c_str(), "connection") == 0) {
            continue;
        }
        os << i.first << ": " << i.second << "\r\n";
    }

    if(!body_.empty()) {
        os << "content-length: " << body_.size() << "\r\n\r\n"
           << body_;
    } else {
        os << "\r\n";
    }
    return os;
}

void HttpRequest::init() {
    std::string conn = getHeader("connection");
    if(!conn.empty()) {
        if(strcasecmp(conn.c_str(), "keep-alive") == 0) {
            close_ = false;
        } else {
            close_ = true;
        }
    }
}

void HttpRequest::initParam() {
    initQueryParam();
    initBodyParam();
    initCookies();
}

void HttpRequest::initQueryParam() {
    if(parserParamFlag_ & 0x1) {
        return;
    }

#define PARSE_PARAM(str, m, flag, trim) \
    size_t pos = 0; \
    do { \
        size_t last = pos; \
        pos = str.find('=', pos); \
        if(pos == std::string::npos) { \
            break; \
        } \
        size_t key = pos; \
        pos = str.find(flag, pos); \
        m.insert(std::make_pair(trim(str.substr(last, key - last)), \
                    fylee::StringUtil::UrlDecode(str.substr(key + 1, pos - key - 1)))); \
        if(pos == std::string::npos) { \
            break; \
        } \
        ++pos; \
    } while(true);

    PARSE_PARAM(query_, params_, '&',);
    parserParamFlag_ |= 0x1;
}

void HttpRequest::initBodyParam() {
    if(parserParamFlag_ & 0x2) {
        return;
    }
    std::string content_type = getHeader("content-type");
    if(strcasestr(content_type.c_str(), "application/x-www-form-urlencoded") == nullptr) {
        parserParamFlag_ |= 0x2;
        return;
    }
    PARSE_PARAM(body_, params_, '&',);
    parserParamFlag_ |= 0x2;
}

void HttpRequest::initCookies() {
    if(parserParamFlag_ & 0x4) {
        return;
    }
    std::string cookie = getHeader("cookie");
    if(cookie.empty()) {
        parserParamFlag_ |= 0x4;
        return;
    }
    PARSE_PARAM(cookie, cookies_, ';', fylee::StringUtil::Trim);
    parserParamFlag_ |= 0x4;
}


HttpResponse::HttpResponse(uint8_t version, bool close)
    :status_(HttpStatus::OK)
    ,version_(version)
    ,close_(close)
    ,websocket_(false) {
}

std::string HttpResponse::getHeader(const std::string& key, const std::string& def) const {
    auto it = headers_.find(key);
    return it == headers_.end() ? def : it->second;
}

void HttpResponse::setHeader(const std::string& key, const std::string& val) {
    headers_[key] = val;
}

void HttpResponse::delHeader(const std::string& key) {
    headers_.erase(key);
}

void HttpResponse::setRedirect(const std::string& uri) {
    status_ = HttpStatus::FOUND;
    setHeader("Location", uri);
}

void HttpResponse::setCookie(const std::string& key, const std::string& val,
                             time_t expired, const std::string& path,
                             const std::string& domain, bool secure) {
    std::stringstream ss;
    ss << key << "=" << val;
    if(expired > 0) {
        ss << ";expires=" << fylee::Time2Str(expired, "%a, %d %b %Y %H:%M:%S") << " GMT";
    }
    if(!domain.empty()) {
        ss << ";domain=" << domain;
    }
    if(!path.empty()) {
        ss << ";path=" << path;
    }
    if(secure) {
        ss << ";secure";
    }
    cookies_.push_back(ss.str());
}

std::string HttpResponse::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

std::ostream& HttpResponse::dump(std::ostream& os) const {
    os << "HTTP/"
       << ((uint32_t)(version_ >> 4))
       << "."
       << ((uint32_t)(version_ & 0x0F))
       << " "
       << (uint32_t)status_
       << " "
       << (reason_.empty() ? HttpStatusToString(status_) : reason_)
       << "\r\n";

    for(auto& i : headers_) {
        if(!websocket_ && strcasecmp(i.first.c_str(), "connection") == 0) {
            continue;
        }
        os << i.first << ": " << i.second << "\r\n";
    }
    for(auto& i : cookies_) {
        os << "Set-Cookie: " << i << "\r\n";
    }
    if(!websocket_) {
        os << "connection: " << (close_ ? "close" : "keep-alive") << "\r\n";
    }
    if(!body_.empty()) {
        os << "content-length: " << body_.size() << "\r\n\r\n"
           << body_;
    } else {
        os << "\r\n";
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const HttpRequest& req) {
    return req.dump(os);
}

std::ostream& operator<<(std::ostream& os, const HttpResponse& rsp) {
    return rsp.dump(os);
}

} // namespace http
} // namespace fylee
