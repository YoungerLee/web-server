#ifndef __FYLEE_HTTP_HTTP_SERVER_H__
#define __FYLEE_HTTP_HTTP_SERVER_H__

#include <memory>
#include "fylee/tcp_server.h"
#include "servlet.h"
#include "http_session.h"

namespace fylee {
class Connection;
class Address;
namespace http {
class HttpServer : public TcpServer {
public:
    typedef std::shared_ptr<HttpServer> ptr;

    HttpServer(EventLoop* loop, const std::shared_ptr<Address> addr, bool keepalive = false, int threads = 1);

    /**
     * @brief 获取ServletDispatch
     */
    ServletDispatch::ptr getServletDispatch() const { return dispatch_;}

    /**
     * @brief 设置ServletDispatch
     */
    void setServletDispatch(ServletDispatch::ptr v) { dispatch_ = v;}

    virtual void setName(const std::string& name) override;

private:
    // 是否支持长连接
    bool isKeepalive_;
    // Servlet分发器
    ServletDispatch::ptr dispatch_;
    void onConnection(const std::shared_ptr<Connection> conn);
    void onMessage(const std::shared_ptr<Connection> conn, uint64_t receiveTime);
    void onWriteComplete(const std::shared_ptr<Connection> conn);
};

}
}
#endif