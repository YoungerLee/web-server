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

/**
 * @brief 文件句柄上下文类
 * @details 管理文件句柄类型(是否socket)
 *          是否阻塞,是否关闭,读/写超时时间
 */
class FdCtx : public std::enable_shared_from_this<FdCtx> {
public:
    typedef std::shared_ptr<FdCtx> ptr;
    /**
     * @brief 通过文件句柄构造FdCtx
     */
    FdCtx(int fd);
    /**
     * @brief 析构函数
     */
    ~FdCtx();

    /**
     * @brief 是否初始化完成
     */
    bool isInit() const { return isInit_;}

    /**
     * @brief 是否socket
     */
    bool isSocket() const { return isSocket_;}

    /**
     * @brief 是否已关闭
     */
    bool isClose() const { return isClosed_;}

    /**
     * @brief 设置用户主动设置非阻塞
     * @param[in] v 是否阻塞
     */
    void setUserNonblock(bool v) { userNonblock_ = v;}

    /**
     * @brief 获取是否用户主动设置的非阻塞
     */
    bool getUserNonblock() const { return userNonblock_;}

    /**
     * @brief 设置系统非阻塞
     * @param[in] v 是否阻塞
     */
    void setSysNonblock(bool v) { sysNonblock_ = v;}

    /**
     * @brief 获取系统非阻塞
     */
    bool getSysNonblock() const { return sysNonblock_;}

    /**
     * @brief 设置超时时间
     * @param[in] type 类型SO_RCVTIMEO(读超时), SO_SNDTIMEO(写超时)
     * @param[in] v 时间毫秒
     */
    void setTimeout(int type, uint64_t v);

    /**
     * @brief 获取超时时间
     * @param[in] type 类型SO_RCVTIMEO(读超时), SO_SNDTIMEO(写超时)
     * @return 超时时间毫秒
     */
    uint64_t getTimeout(int type);
private:
    /**
     * @brief 初始化
     */
    bool init();
private:
    /// 是否初始化
    bool isInit_: 1;
    /// 是否socket
    bool isSocket_: 1;
    /// 是否hook非阻塞
    bool sysNonblock_: 1;
    /// 是否用户主动设置非阻塞
    bool userNonblock_: 1;
    /// 是否关闭
    bool isClosed_: 1;
    /// 文件句柄
    int fd_;
    /// 读超时时间毫秒
    uint64_t recvTimeout_;
    /// 写超时时间毫秒
    uint64_t sendTimeout_;
};

/**
 * @brief 文件句柄管理类
 */
class FdManager {
public:
    typedef RWMutex RWMutexType;
    /**
     * @brief 无参构造函数
     */
    FdManager();

    /**
     * @brief 获取/创建文件句柄类FdCtx
     * @param[in] fd 文件句柄
     * @param[in] auto_create 是否自动创建
     * @return 返回对应文件句柄类FdCtx::ptr
     */
    FdCtx::ptr get(int fd, bool auto_create = false);

    /**
     * @brief 删除文件句柄类
     * @param[in] fd 文件句柄
     */
    void del(int fd);
private:
    /// 读写锁
    RWMutexType mutex_;
    /// 文件句柄集合
    std::vector<FdCtx::ptr> datas_;
};

/// 文件句柄单例
typedef Singleton<FdManager> FdMgr;

}

#endif
