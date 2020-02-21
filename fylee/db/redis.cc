#include "redis.h"
#include "fylee/log.h"
#include "fylee/util.h"
namespace fylee {
static fylee::Logger::ptr g_logger = LOG_NAME("system");
std::string Redis::getValueByKey(const std::map<std::string, std::string>& conf, 
                                 const std::string& key,
                                 const std::string& def) {
    auto it = conf.find(key);
    return it == conf.end() ? def : it->second;
}
Redis::Redis(const std::map<std::string, std::string>& conf) {
    type_ = IRedis::REDIS;
    std::string tmp = getValueByKey(conf, "host");
    size_t pos = tmp.find(":");
    host_ = tmp.substr(0, pos);
    port_ = fylee::TypeUtil::Atoi(tmp.substr(pos + 1));
    logEnable_ = fylee::TypeUtil::Atoi(getValueByKey(conf, "log_enable", "1"));

    tmp = getValueByKey(conf, "timeout_com");
    if(tmp.empty()) {
        tmp = getValueByKey(conf, "timeout");
    }
    uint64_t ms = fylee::TypeUtil::Atoi(tmp);
    cmdTimeout_.tv_sec = ms / 1000;
    cmdTimeout_.tv_usec = ms % 1000 * 1000;
}

bool Redis::connect() {
    return connect(host_, port_, 50);
}

bool Redis::connect(const std::string& ip, uint32_t port, uint64_t ms) {
    host_ = ip;
    port_ = port;
    connMs_ = ms;
    if(context_) return true;
    timeval tv = { (int)ms / 1000, (int)ms % 1000 * 1000 };
    auto ctx = redisConnectWithTimeout(ip.c_str(), port, tv);
    if (ctx){
        if(cmdTimeout_.tv_sec || cmdTimeout_.tv_usec) {
            setTimeout(cmdTimeout_.tv_sec * 1000 + cmdTimeout_.tv_usec / 1000);
        }
        context_.reset(ctx, redisFree);
        return true;
    }
    return false;
}

bool Redis::reconnect() {
    return redisReconnect(context_.get());
}

bool Redis::setTimeout(uint64_t ms) {
    cmdTimeout_.tv_sec = ms / 1000;
    cmdTimeout_.tv_usec = ms % 1000 * 1000;
    return redisSetTimeout(context_.get(), cmdTimeout_);
}

ReplyPtr Redis::cmd(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ReplyPtr rt = cmd(fmt, ap);
    va_end(ap);
    return rt;
}

ReplyPtr Redis::cmd(const char* fmt, va_list ap) {
    redisReply* reply = (redisReply*)redisvCommand(context_.get(), fmt, ap);
    if(!reply) {
        if(logEnable_) {
            LOG_ERROR(g_logger) << "redisCommand error: (" << fmt << ")(" << host_ << ":" << port_ << ")(" << name_ << ")";
        }
        return nullptr;
    }
    ReplyPtr rt(reply, freeReplyObject);
    if(reply->type != REDIS_REPLY_ERROR) {
        return rt;
    }
    if(logEnable_) {
        LOG_ERROR(g_logger) << "redisCommand error: (" << fmt << ")(" << host_ << ":" << port_ << ")(" << name_ << ")"
                    << ": " << reply->str;
    }
    return nullptr;
}

ReplyPtr Redis::cmd(const std::vector<std::string>& argv) {
    std::vector<const char*> args;
    std::vector<size_t> lens;
    for(auto& i : argv) {
        args.push_back(i.c_str());
        lens.push_back(i.size());
    }

    redisReply* reply = (redisReply*)redisCommandArgv(context_.get(), argv.size(), &args[0], &lens[0]);
    if(!reply) {
        if(logEnable_) {
            LOG_ERROR(g_logger) << "redisCommandArgv error: (" << host_ << ":" << port_ << ")(" << name_ << ")";
        }
        return nullptr;
    }
    ReplyPtr rt(reply, freeReplyObject);
    if(reply->type != REDIS_REPLY_ERROR) {
        return rt;
    }
    if(logEnable_) {
        LOG_ERROR(g_logger) << "redisCommandArgv error: (" << host_ << ":" << port_ << ")(" << name_ << ")"
                    << reply->str;
    }
    return nullptr;
}

ReplyPtr Redis::getReply() {
    redisReply* reply = nullptr;
    if(redisGetReply(context_.get(), (void**)&reply) == REDIS_OK) {
        ReplyPtr rt(reply, freeReplyObject);
        return rt;
    }
    if(logEnable_) {
        LOG_ERROR(g_logger) << "redisGetReply error: (" << host_ << ":" << port_ << ")(" << name_ << ")";
    }
    return nullptr;
}

int Redis::appendCmd(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int rt = appendCmd(fmt, ap);
    va_end(ap);
    return rt;

}

int Redis::appendCmd(const char* fmt, va_list ap) {
    return redisvAppendCommand(context_.get(), fmt, ap);
}

int Redis::appendCmd(const std::vector<std::string>& argv) {
    std::vector<const char*> args;
    std::vector<size_t> lens;
    for(auto& i : argv) {
        args.push_back(i.c_str());
        lens.push_back(i.size());
    }
    return redisAppendCommandArgv(context_.get(), argv.size(), &args[0], &lens[0]);
}

}
