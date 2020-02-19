#include "fylee/http/http_server.h"
#include "fylee/log.h"
#include "fylee/eventloop.h"

fylee::Logger::ptr g_logger = LOG_ROOT();

void run(fylee::EventLoop* loop) {
    auto addr = fylee::Address::LookupAnyIPAddress("0.0.0.0:8080");
    fylee::http::HttpServer::ptr server(new fylee::http::HttpServer(loop, addr, false, 1));
    server->start();
    loop->loop();
}

int main(int argc, char** argv) {
    fylee::EventLoop loop;
    run(&loop);
    return 0;
}
