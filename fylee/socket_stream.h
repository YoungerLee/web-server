#ifndef __FYLEE_SOCKET_STREAM_H__
#define __FYLEE_SOCKET_STREAM_H__

#include "stream.h"
#include "socket.h"
#include "mutex.h"

namespace fylee {

class SocketStream : public Stream {
public:
    typedef std::shared_ptr<SocketStream> ptr;

    SocketStream(Socket::ptr sock, bool owner = true);

    ~SocketStream();

    virtual int read(void* buffer, size_t length) override;

    virtual int read(Buffer::ptr ba, size_t length) override;

    virtual int write(const void* buffer, size_t length) override;

    virtual int write(Buffer::ptr ba, size_t length) override;

    virtual void close() override;

    Socket::ptr getSocket() const { return socket_;}

    bool isConnected() const;

    Address::ptr getRemoteAddress();
    Address::ptr getLocalAddress();
    std::string getRemoteAddressString();
    std::string getLocalAddressString();
protected:
    // Socket类
    Socket::ptr socket_;
    // 是否主控
    bool owner_;
};

}

#endif
