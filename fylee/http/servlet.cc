#include "servlet.h"
#include <fnmatch.h>

namespace fylee {
namespace http {

FunctionServlet::FunctionServlet(Callback cb)
    :Servlet("FunctionServlet")
    ,cb_(cb) {
}

int32_t FunctionServlet::handle(fylee::http::HttpRequest::ptr request, 
                                fylee::http::HttpResponse::ptr response, 
                                fylee::http::HttpSession::ptr session) {
    return cb_(request, response, session);
}



ServletDispatch::ServletDispatch()
    :Servlet("ServletDispatch") {
    default_.reset(new NotFoundServlet("fylee/1.0")); // 默认not found servlet
}

int32_t ServletDispatch::handle(fylee::http::HttpRequest::ptr request, 
                                fylee::http::HttpResponse::ptr response, 
                                fylee::http::HttpSession::ptr session) {
    auto slt = getMatchedServlet(request->getPath());
    if(slt) {
        slt->handle(request, response, session);
    }
    return 0;
}

void ServletDispatch::addServlet(const std::string& uri, Servlet::ptr slt) {
    RWMutexType::WriteLock lock(mutex_);
    datas_[uri] = std::make_shared<HoldServletCreator>(slt);
}

void ServletDispatch::addServletCreator(const std::string& uri, IServletCreator::ptr creator) {
    RWMutexType::WriteLock lock(mutex_);
    datas_[uri] = creator;
}


void ServletDispatch::addServlet(const std::string& uri, 
                                 FunctionServlet::Callback cb) {
    RWMutexType::WriteLock lock(mutex_);
    datas_[uri] = std::make_shared<HoldServletCreator>(std::make_shared<FunctionServlet>(cb));
}

void ServletDispatch::delServlet(const std::string& uri) {
    RWMutexType::WriteLock lock(mutex_);
    datas_.erase(uri);
}

Servlet::ptr ServletDispatch::getServlet(const std::string& uri) {
    RWMutexType::ReadLock lock(mutex_);
    auto it = datas_.find(uri);
    return it == datas_.end() ? nullptr : it->second->get();
}

Servlet::ptr ServletDispatch::getMatchedServlet(const std::string& uri) {
    RWMutexType::ReadLock lock(mutex_);
    if(datas_.find(uri) != datas_.end()) {
        return datas_[uri]->get();
    }
    return default_;
}

void ServletDispatch::listAllServletCreator(std::map<std::string, IServletCreator::ptr>& infos) {
    RWMutexType::ReadLock lock(mutex_);
    for(auto& i : datas_) {
        infos[i.first] = i.second;
    }
}

NotFoundServlet::NotFoundServlet(const std::string& name)
    :Servlet("NotFoundServlet"), 
     name_(name) {
    content_ = "<html><head><title>404 Not Found"
        "</title></head><body><center><h1>404 Not Found</h1></center>"
        "<hr><center>" + name + "</center></body></html>";

}

int32_t NotFoundServlet::handle(fylee::http::HttpRequest::ptr request, 
                                fylee::http::HttpResponse::ptr response, 
                                fylee::http::HttpSession::ptr session) {
    response->setStatus(fylee::http::HttpStatus::NOT_FOUND);
    response->setHeader("Server", "fylee/1.0.0");
    response->setHeader("Content-Type", "text/html");
    response->setBody(content_);
    return 0;
}


}
}
