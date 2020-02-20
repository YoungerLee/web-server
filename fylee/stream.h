#ifndef __FYLEE_STREAM_H__
#define __FYLEE_STREAM_H__

#include <memory>
#include "buffer.h"

namespace fylee {

class Stream {
public:
    typedef std::shared_ptr<Stream> ptr;
    virtual ~Stream() {}

    virtual int read(void* buffer, size_t length) = 0;

    virtual int read(Buffer::ptr ba, size_t length) = 0;

    virtual int readFixSize(void* buffer, size_t length);

    virtual int readFixSize(Buffer::ptr ba, size_t length);

    virtual int write(const void* buffer, size_t length) = 0;

    virtual int write(Buffer::ptr ba, size_t length) = 0;

    virtual int writeFixSize(const void* buffer, size_t length);

    virtual int writeFixSize(Buffer::ptr ba, size_t length);

    virtual void close() = 0;
};

}

#endif
