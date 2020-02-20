#ifndef __FYLEE_HTTP_SERVLET_H__
#define __FYLEE_HTTP_SERVLET_H__

#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include "http.h"
#include "http_session.h"
#include "fylee/thread.h"
#include "fylee/util.h"

namespace fylee {
namespace http {

class Servlet {
public:
    typedef std::shared_ptr<Servlet> ptr;

    Servlet(const std::string& name)
    :name_(name) {}

    virtual ~Servlet() {}

    virtual int32_t handle(fylee::http::HttpRequest::ptr request, 
                           fylee::http::HttpResponse::ptr response, 
                           fylee::http::HttpSession::ptr session) = 0;
                   
   
    const std::string& getName() const { return name_;}
protected:

    std::string name_;
};

class FunctionServlet : public Servlet {
public:
    typedef std::shared_ptr<FunctionServlet> ptr;
    typedef std::function<int32_t (fylee::http::HttpRequest::ptr request, 
                                   fylee::http::HttpResponse::ptr response, 
                                   fylee::http::HttpSession::ptr session)> Callback;

    FunctionServlet(Callback cb);
    virtual int32_t handle(fylee::http::HttpRequest::ptr request, 
                           fylee::http::HttpResponse::ptr response, 
                           fylee::http::HttpSession::ptr session) override;
private:
    Callback cb_;
};

class IServletCreator {
public:
    typedef std::shared_ptr<IServletCreator> ptr;
    virtual ~IServletCreator() {}
    virtual Servlet::ptr get() const = 0;
    virtual std::string getName() const = 0;
};

class HoldServletCreator : public IServletCreator {
public:
    typedef std::shared_ptr<HoldServletCreator> ptr;
    HoldServletCreator(Servlet::ptr slt)
        :servlet_(slt) {
    }

    Servlet::ptr get() const override {
        return servlet_;
    }

    std::string getName() const override {
        return servlet_->getName();
    }
private:
    Servlet::ptr servlet_;
};

template<class T>
class ServletCreator : public IServletCreator {
public:
    typedef std::shared_ptr<ServletCreator> ptr;

    ServletCreator() {
    }

    Servlet::ptr get() const override {
        return Servlet::ptr(new T);
    }

    std::string getName() const override {
        return TypeToName<T>();
    }
};

class ServletDispatch : public Servlet {
public:
    typedef std::shared_ptr<ServletDispatch> ptr;
    typedef RWMutex RWMutexType;

    ServletDispatch();
    virtual int32_t handle(fylee::http::HttpRequest::ptr request, 
                           fylee::http::HttpResponse::ptr response, 
                           fylee::http::HttpSession::ptr session) override;

    void addServlet(const std::string& uri, Servlet::ptr slt);

    void addServlet(const std::string& uri, FunctionServlet::Callback cb);


    void addServletCreator(const std::string& uri, IServletCreator::ptr creator);

    template<class T>
    void addServletCreator(const std::string& uri) {
        addServletCreator(uri, std::make_shared<ServletCreator<T> >());
    }

    void delServlet(const std::string& uri);

    Servlet::ptr getDefault() const { return default_;}

    void setDefault(Servlet::ptr v) { default_ = v;}

    Servlet::ptr getServlet(const std::string& uri);

     
    Servlet::ptr getMatchedServlet(const std::string& uri);

    void listAllServletCreator(std::map<std::string, IServletCreator::ptr>& infos);
private:
    RWMutexType mutex_;
    std::unordered_map<std::string, IServletCreator::ptr> datas_;
    Servlet::ptr default_;
};

class NotFoundServlet : public Servlet {
public:
    typedef std::shared_ptr<NotFoundServlet> ptr;
    NotFoundServlet(const std::string& name);
    virtual int32_t handle(fylee::http::HttpRequest::ptr request, 
                           fylee::http::HttpResponse::ptr response, 
                           fylee::http::HttpSession::ptr session) override;

private:
    std::string name_;
    std::string content_;
};

}
}

#endif
