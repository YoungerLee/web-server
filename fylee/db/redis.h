#ifndef __FYLEE_REDIS_H__
#define __FYLEE_REDIS_H__
#include <hiredis/hiredis.h>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <sys/time.h>
#include <fylee/singleton.h>

namespace fylee {
typedef std::shared_ptr<redisReply> ReplyPtr;
typedef std::shared_ptr<redisContext> CtxPtr;
class IRedis {
public:
    enum Type {
        REDIS,
        REDIS_CLUSTER
    };
    typedef std::shared_ptr<IRedis> ptr;
    IRedis() : logEnable_(true) { }
    virtual ~IRedis() { }

    virtual ReplyPtr cmd(const char* fmt, ...) = 0;
    virtual ReplyPtr cmd(const char* fmt, va_list ap) = 0;
    virtual ReplyPtr cmd(const std::vector<std::string>& argv) = 0;

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    Type getType() const { return type_; }
protected:
    std::string name_;
    Type type_;
    bool logEnable_;
};

class ISyncRedis : public IRedis {
public:
    typedef std::shared_ptr<ISyncRedis> ptr;
    virtual ~ISyncRedis() { };

    virtual bool connect() = 0;
    virtual bool connect(const std::string& ip, uint32_t port, uint64_t ms) = 0;
    virtual bool reconnect() = 0;
    virtual bool setTimeout(uint64_t ms) = 0;

    virtual int appendCmd(const char* fmt, ...) = 0;
    virtual int appendCmd(const char* fmt, va_list ap) = 0;
    virtual int appendCmd(const std::vector<std::string>& argv) = 0;

    virtual ReplyPtr getReply() = 0;

    uint64_t getLastActiveTime() const { return lastActiveTime_; }
    void setLastActiveTime(uint64_t time) { lastActiveTime_ = time; }
protected:
    uint64_t lastActiveTime_;
};

class Redis : public ISyncRedis {
public:
    typedef std::shared_ptr<Redis> ptr;
    Redis() { type_ = IRedis::REDIS; }
    Redis(const std::map<std::string, std::string>& conf);

    virtual ~Redis() { };
    virtual bool connect();
    virtual bool connect(const std::string& ip, uint32_t port, uint64_t ms);
    virtual bool reconnect();
    virtual bool setTimeout(uint64_t ms);

    virtual ReplyPtr cmd(const char* fmt, ...);
    virtual ReplyPtr cmd(const char* fmt, va_list ap);
    virtual ReplyPtr cmd(const std::vector<std::string>& argv);

     virtual int appendCmd(const char* fmt, ...);
    virtual int appendCmd(const char* fmt, va_list ap);
    virtual int appendCmd(const std::vector<std::string>& argv);

    virtual ReplyPtr getReply();
private:
    static std::string getValueByKey(const std::map<std::string, std::string>& conf, 
                                     const std::string& key, 
                                     const std::string& def = "");
private:
    std::string host_;
    uint32_t port_;
    uint64_t connMs_;
    struct timeval cmdTimeout_;
    CtxPtr context_;
};
}
#endif