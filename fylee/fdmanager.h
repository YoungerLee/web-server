#ifndef __FYLEE_FDMANAGER_H__
#define __FYLEE_FDMANAGER_H__

#include <memory>
#include <vector>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "thread.h"
#include "singleton.h"

namespace fylee {

class FdCtx : public std::enable_shared_from_this<FdCtx> {
public:
    typedef std::shared_ptr<FdCtx> ptr;

    FdCtx(int fd);
   
    ~FdCtx();

    bool isInit() const { return isInit_;}

    bool isSocket() const { return isSocket_;}

    bool isClose() const { return isClosed_;}

    void setUserNonblock(bool v) { userNonblock_ = v;}

    bool getUserNonblock() const { return userNonblock_;}

    void setSysNonblock(bool v) { sysNonblock_ = v;}

    bool getSysNonblock() const { return sysNonblock_;}

    void setTimeout(int type, uint64_t v);

    uint64_t getTimeout(int type);
private:
    bool init();
private:
    bool isInit_: 1;
    bool isSocket_: 1;
    bool sysNonblock_: 1;
    bool userNonblock_: 1;
    bool isClosed_: 1;
    int fd_;
    uint64_t recvTimeout_;
    uint64_t sendTimeout_;
};

class FdManager {
public:
    typedef RWMutex RWMutexType;

    FdManager();

    FdCtx::ptr get(int fd, bool auto_create = false);

    void del(int fd);
private:
    RWMutexType mutex_;
    std::vector<FdCtx::ptr> datas_;
};

typedef Singleton<FdManager> FdMgr;

}

#endif
