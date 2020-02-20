#ifndef __FYLEE_URI_H__
#define __FYLEE_URI_H__

#include <memory>
#include <string>
#include <stdint.h>
#include "address.h"

namespace fylee {

/*
     foo://user@fylee.com:8042/over/there?name=ferret#nose
       \_/   \______________/\_________/ \_________/ \__/
        |           |            |            |        |
     scheme     authority       path        query   fragment
*/

class Uri {
public:
    /// 智能指针类型定义
    typedef std::shared_ptr<Uri> ptr;

    static Uri::ptr Create(const std::string& uri);

    Uri();

    const std::string& getScheme() const { return scheme_;}

    const std::string& getUserinfo() const { return userinfo_;}

    const std::string& getHost() const { return host_;}

    const std::string& getPath() const;

    const std::string& getQuery() const { return query_;}

    const std::string& getFragment() const { return fragment_;}

    int32_t getPort() const;

    void setScheme(const std::string& v) { scheme_ = v;}

    void setUserinfo(const std::string& v) { userinfo_ = v;}

    void setHost(const std::string& v) { host_ = v;}

    void setPath(const std::string& v) { path_ = v;}

    void setQuery(const std::string& v) { query_ = v;}

    /**
     * @brief 设置fragment
     * @param v fragment
     */
    void setFragment(const std::string& v) { fragment_ = v;}

    void setPort(int32_t v) { port_ = v;}

    std::ostream& dump(std::ostream& os) const;

    std::string toString() const;

    Address::ptr createAddress() const;
private:
    bool isDefaultPort() const;
private:
    // schema
    std::string scheme_;
    // 用户信息
    std::string userinfo_;
    // host
    std::string host_;
    // 路径
    std::string path_;
    // 查询参数
    std::string query_;
    // fragment
    std::string fragment_;
    // 端口
    int32_t port_;
};

}

#endif
