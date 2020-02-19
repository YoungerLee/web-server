#include "fylee/tcp_server.h"
#include "fylee/log.h"
#include "fylee/buffer.h"
#include "fylee/eventloop.h"
#include "fylee/address.h"
#include "fylee/connection.h"

using namespace fylee;
using namespace std::placeholders;
int numThreads = 0;

static fylee::Logger::ptr g_logger = LOG_ROOT();
class EchoServer : public TcpServer {
public:
    EchoServer(EventLoop* loop, const Address::ptr listenAddr)
        :TcpServer(loop, listenAddr, "EchoServer") {
        setConnectionCallback(std::bind(&EchoServer::onConnection, this, _1));
        setMessageCallback(std::bind(&EchoServer::onMessage, this, _1, _2));
        setThreadNum(numThreads);
    }

private:
    void onConnection(const Connection::ptr conn) {
        if(conn->isConnected()) {
            // LOG_INFO(g_logger) << conn->getSocket()->toString() << " is " 
            //                << (conn->isConnected() ? "UP" : "DOWN");

            conn->send("hello\n");
        }
    }

    void onMessage(const Connection::ptr conn, uint64_t timestamp) {
        Buffer::ptr buf = conn->inputBuffer();
        buf->setPosition(0);
        std::string msg(buf->toString());
        buf->clear();
        LOG_INFO(g_logger) << conn->getName() << " recv " << msg << " bytes at " << timestamp;
        if (msg == "exit\n") {
            conn->send("bye\n");
            conn->shutdown();
        }
        EventLoop* loop = TcpServer::getLoop();
        if (msg == "quit\n") {
            loop->quit();
        }
        conn->send(msg);
    }
};

int main(int argc, char* argv[]) {
    LOG_INFO(g_logger) << "pid = " << getpid() << ", tid = " << fylee::GetThreadId();
    LOG_INFO(g_logger) << "sizeof(Connection) = " << sizeof(Connection);
    if (argc > 1) {
        numThreads = atoi(argv[1]);
    }
    EventLoop loop;
    auto addr = Address::LookupAny("0.0.0.0:8888");
    std::shared_ptr<EchoServer> server(new EchoServer(&loop, addr));

    server->start();
    loop.loop();
}
