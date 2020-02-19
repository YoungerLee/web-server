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

/**
 * @brief Servlet封装
 */
class Servlet {
public:
    /// 智能指针类型定义
    typedef std::shared_ptr<Servlet> ptr;

    /**
     * @brief 构造函数
     * @param[in] name 名称
     */
    Servlet(const std::string& name)
    :name_(name) {}

    /**
     * @brief 析构函数
     */
    virtual ~Servlet() {}

    /**
     * @brief 处理请求
     * @param[in] request HTTP请求
     * @param[in] response HTTP响应
     * @param[in] session HTTP连接
     * @return 是否处理成功
     */
    virtual int32_t handle(fylee::http::HttpRequest::ptr request, 
                           fylee::http::HttpResponse::ptr response, 
                           fylee::http::HttpSession::ptr session) = 0;
                   
    /**
     * @brief 返回Servlet名称
     */
    const std::string& getName() const { return name_;}
protected:
    /// 名称
    std::string name_;
};

/**
 * @brief 函数式Servlet
 */
class FunctionServlet : public Servlet {
public:
    /// 智能指针类型定义
    typedef std::shared_ptr<FunctionServlet> ptr;
    /// 函数回调类型定义
    typedef std::function<int32_t (fylee::http::HttpRequest::ptr request, 
                                   fylee::http::HttpResponse::ptr response, 
                                   fylee::http::HttpSession::ptr session)> Callback;


    /**
     * @brief 构造函数
     * @param[in] cb 回调函数
     */
    FunctionServlet(Callback cb);
    virtual int32_t handle(fylee::http::HttpRequest::ptr request, 
                           fylee::http::HttpResponse::ptr response, 
                           fylee::http::HttpSession::ptr session) override;
private:
    /// 回调函数
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

/**
 * @brief Servlet分发器
 */
class ServletDispatch : public Servlet {
public:
    /// 智能指针类型定义
    typedef std::shared_ptr<ServletDispatch> ptr;
    /// 读写锁类型定义
    typedef RWMutex RWMutexType;

    /**
     * @brief 构造函数
     */
    ServletDispatch();
    virtual int32_t handle(fylee::http::HttpRequest::ptr request, 
                           fylee::http::HttpResponse::ptr response, 
                           fylee::http::HttpSession::ptr session) override;

    /**
     * @brief 添加servlet
     * @param[in] uri uri
     * @param[in] slt serlvet
     */
    void addServlet(const std::string& uri, Servlet::ptr slt);

    /**
     * @brief 添加servlet
     * @param[in] uri uri
     * @param[in] cb FunctionServlet回调函数
     */
    void addServlet(const std::string& uri, FunctionServlet::Callback cb);

    /**
     * @brief 添加模糊匹配servlet
     * @param[in] uri uri 模糊匹配 /fylee_*
     * @param[in] slt servlet
     */
    void addGlobServlet(const std::string& uri, Servlet::ptr slt);

    /**
     * @brief 添加模糊匹配servlet
     * @param[in] uri uri 模糊匹配 /fylee_*
     * @param[in] cb FunctionServlet回调函数
     */
    void addGlobServlet(const std::string& uri, FunctionServlet::Callback cb);

    void addServletCreator(const std::string& uri, IServletCreator::ptr creator);
    void addGlobServletCreator(const std::string& uri, IServletCreator::ptr creator);

    template<class T>
    void addServletCreator(const std::string& uri) {
        addServletCreator(uri, std::make_shared<ServletCreator<T> >());
    }

    template<class T>
    void addGlobServletCreator(const std::string& uri) {
        addGlobServletCreator(uri, std::make_shared<ServletCreator<T> >());
    }

    /**
     * @brief 删除servlet
     * @param[in] uri uri
     */
    void delServlet(const std::string& uri);

    /**
     * @brief 删除模糊匹配servlet
     * @param[in] uri uri
     */
    void delGlobServlet(const std::string& uri);

    /**
     * @brief 返回默认servlet
     */
    Servlet::ptr getDefault() const { return default_;}

    /**
     * @brief 设置默认servlet
     * @param[in] v servlet
     */
    void setDefault(Servlet::ptr v) { default_ = v;}


    /**
     * @brief 通过uri获取servlet
     * @param[in] uri uri
     * @return 返回对应的servlet
     */
    Servlet::ptr getServlet(const std::string& uri);

    /**
     * @brief 通过uri获取模糊匹配servlet
     * @param[in] uri uri
     * @return 返回对应的servlet
     */
    Servlet::ptr getGlobServlet(const std::string& uri);

    /**
     * @brief 通过uri获取servlet
     * @param[in] uri uri
     * @return 优先精准匹配,其次模糊匹配,最后返回默认
     */
    Servlet::ptr getMatchedServlet(const std::string& uri);

    void listAllServletCreator(std::map<std::string, IServletCreator::ptr>& infos);
    void listAllGlobServletCreator(std::map<std::string, IServletCreator::ptr>& infos);
private:
    /// 读写互斥量
    RWMutexType mutex_;
    /// 精准匹配servlet MAP
    /// uri(/fylee/xxx) -> servlet
    std::unordered_map<std::string, IServletCreator::ptr> datas_;
    /// 模糊匹配servlet 数组
    /// uri(/fylee/*) -> servlet
    std::vector<std::pair<std::string, IServletCreator::ptr> > globs_;
    /// 默认servlet，所有路径都没匹配到时使用
    Servlet::ptr default_;
};

/**
 * @brief NotFoundServlet(默认返回404)
 */
class NotFoundServlet : public Servlet {
public:
    /// 智能指针类型定义
    typedef std::shared_ptr<NotFoundServlet> ptr;
    /**
     * @brief 构造函数
     */
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
