#include "http_server.h"
#include "http_session.h"
#include "fylee/socket.h"
#include "fylee/address.h"
#include "fylee/log.h"
#include "fylee/macro.h"
#include "fylee/connection.h"

namespace fylee {
namespace http {
using namespace std::placeholders;

static fylee::Logger::ptr g_logger = LOG_NAME("system");

HttpServer::HttpServer(EventLoop* loop, 
    const Address::ptr addr, bool keepalive, int threads) 
    :TcpServer(loop, addr, "HttpServer"), 
    isKeepalive_(keepalive) {
    dispatch_.reset(new ServletDispatch);
    setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, _1));
    setMessageCallback(
        std::bind(&HttpServer::onMessage, this, _1, _2));
    setThreadNum(threads);
}

void HttpServer::setName(const std::string& name) {
    TcpServer::setName(name);
    dispatch_->setDefault(std::make_shared<NotFoundServlet>(name));

}

void HttpServer::onConnection(const Connection::ptr conn) {
    if (conn->isConnected()) {
        LOG_INFO(g_logger) << "connection: " << conn->getName() << "established. ";
    }
}

void HttpServer::onWriteComplete(const Connection::ptr conn) {

}

void HttpServer::onMessage(const Connection::ptr conn, uint64_t receiveTime) {
    auto client = conn->getSocket();
    LOG_DEBUG(g_logger) << "handleClient " << *client << " at time: " << receiveTime;
    HttpSession::ptr session = std::dynamic_pointer_cast<HttpSession>(conn->getStream());
    bool isErr = false;
    do {
        auto req = session->recvRequest();
        if(!req) {
            isErr = true;
            LOG_DEBUG(g_logger) << "recv http request fail, errno="
                << errno << " errstr=" << strerror(errno)
                << " cliet:" << *client << " keep_alive=" << isKeepalive_;
            break;
        }
        HttpResponse::ptr rsp(new HttpResponse(req->getVersion(), req->isClose() || !isKeepalive_));
        rsp->setHeader("Server", getName());
        dispatch_->handle(req, rsp, session);
        conn->send(rsp->toString());
        if(!isKeepalive_ || req->isClose()) {
            break;
        }
    } while(true);
    if(isErr) {
        conn->forceClose();
    } else {
        conn->shutdown();
    }
}
}
}
