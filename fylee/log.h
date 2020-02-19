#ifndef __FYLEE_LOG_H__
#define __FYLEE_LOG_H__

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <stdarg.h>
#include <map>
#include "util.h"
#include "singleton.h"
#include "thread.h"

#define LOG_LEVEL(logger, level) \
    if(logger->getLevel() <= level) \
        fylee::LogEventWrap(fylee::LogEvent::ptr(new fylee::LogEvent(logger, level, \
                        __FILE__, __LINE__, 0, fylee::GetThreadId(),\
                fylee::GetCoroId(), time(0), fylee::Thread::GetName()))).getSS()

#define LOG_DEBUG(logger) LOG_LEVEL(logger, fylee::LogLevel::DEBUG)

#define LOG_INFO(logger) LOG_LEVEL(logger, fylee::LogLevel::INFO)

#define LOG_WARN(logger) LOG_LEVEL(logger, fylee::LogLevel::WARN)

#define LOG_ERROR(logger) LOG_LEVEL(logger, fylee::LogLevel::ERROR)

#define LOG_FATAL(logger) LOG_LEVEL(logger, fylee::LogLevel::FATAL)

#define LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLevel() <= level) \
        fylee::LogEventWrap(fylee::LogEvent::ptr(new fylee::LogEvent(logger, level, \
                        __FILE__, __LINE__, 0, fylee::GetThreadId(),\
                fylee::GetCoroId(), time(0), fylee::Thread::GetName()))).getEvent()->format(fmt, __VA_ARGS__)

#define LOG_FMT_DEBUG(logger, fmt, ...) LOG_FMT_LEVEL(logger, fylee::LogLevel::DEBUG, fmt, __VA_ARGS__)

#define LOG_FMT_INFO(logger, fmt, ...)  LOG_FMT_LEVEL(logger, fylee::LogLevel::INFO, fmt, __VA_ARGS__)

#define LOG_FMT_WARN(logger, fmt, ...)  LOG_FMT_LEVEL(logger, fylee::LogLevel::WARN, fmt, __VA_ARGS__)

#define LOG_FMT_ERROR(logger, fmt, ...) LOG_FMT_LEVEL(logger, fylee::LogLevel::ERROR, fmt, __VA_ARGS__)

#define LOG_FMT_FATAL(logger, fmt, ...) LOG_FMT_LEVEL(logger, fylee::LogLevel::FATAL, fmt, __VA_ARGS__)

#define LOG_ROOT() fylee::LoggerMgr::GetInstance()->getRoot()

#define LOG_NAME(name) fylee::LoggerMgr::GetInstance()->getLogger(name)

namespace fylee {
class Logger;
class LoggerManager;

class LogLevel {
public:
    enum Level {
        UNKNOW = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };

    static const char* ToString(LogLevel::Level level);

    static LogLevel::Level FromString(const std::string& str);
};

class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;
    /**
     * @brief 构造函数
     * @param[in] logger 日志器
     * @param[in] level 日志级别
     * @param[in] file 文件名
     * @param[in] line 文件行号
     * @param[in] elapse 程序启动依赖的耗时(毫秒)
     * @param[in] thread_id 线程id
     * @param[in] fiber_id 协程id
     * @param[in] time 日志事件(秒)
     * @param[in] thread_name 线程名称
     */
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level
            ,const char* file, int32_t lineno, uint32_t elapse
            ,uint32_t thread_id, uint32_t fiber_id, uint64_t time
            ,const std::string& thread_name);

    const char* getFile() const { return file_;}

    int32_t getLine() const { return lineno_;}

    uint32_t getElapse() const { return elapse_;}

    uint32_t getThreadId() const { return tid_;}

    uint32_t getFiberId() const { return fiber_id_;}

    uint64_t getTime() const { return time_;}

    const std::string& getThreadName() const { return thread_name_;}

    std::string getContent() const { return ss_.str();}

    std::shared_ptr<Logger> getLogger() const { return logger_;}

    LogLevel::Level getLevel() const { return level_;}

    std::stringstream& getSS() { return ss_;}

    void format(const char* fmt, ...);

    void format(const char* fmt, va_list al);
private:
    const char* file_ = nullptr;
    int32_t lineno_ = 0;
    uint32_t elapse_ = 0;
    uint32_t tid_ = 0;
    uint32_t fiber_id_ = 0;
    uint64_t time_ = 0;
    std::string thread_name_;
    std::stringstream ss_;
    std::shared_ptr<Logger> logger_;
    LogLevel::Level level_;
};

class LogEventWrap {
public:
    LogEventWrap(LogEvent::ptr event);

    ~LogEventWrap();

    LogEvent::ptr getEvent() const { return event_;}

    std::stringstream& getSS();
private:
    LogEvent::ptr event_;
};

class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    /**
     * @brief 构造函数
     * @param[in] pattern 格式模板
     * @details 
     *  %m 消息
     *  %p 日志级别
     *  %r 累计毫秒数
     *  %c 日志名称
     *  %t 线程id
     *  %n 换行
     *  %d 时间
     *  %f 文件名
     *  %l 行号
     *  %T 制表符
     *  %C 协程id
     *  %N 线程名称
     *
     *  默认格式 "%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%C%T[%p]%T[%c]%T%f:%l%T%m%n"
     */
    LogFormatter(const std::string& pattern);

    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
    std::ostream& format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
public:
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        virtual ~FormatItem() {}
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
    };

    void init();

    bool isError() const { return error_;}

    const std::string getPattern() const { return pattern_;}
private:
    std::string pattern_;
    std::vector<FormatItem::ptr> items_;
    bool error_ = false;

};

class LogAppender {
friend class Logger;
public:
    typedef std::shared_ptr<LogAppender> ptr;
    typedef SpinLock MutexType;

    virtual ~LogAppender() {}

    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

    virtual std::string toYamlString() = 0;

    void setFormatter(LogFormatter::ptr val);

    LogFormatter::ptr getFormatter();

    LogLevel::Level getLevel() const { return level_;}

    void setLevel(LogLevel::Level val) { level_ = val;}
protected:
    LogLevel::Level level_ = LogLevel::DEBUG;
    bool hasFormatter_ = false;
    MutexType mutex_;
    LogFormatter::ptr formatter_;
};


class Logger : public std::enable_shared_from_this<Logger> {
friend class LoggerManager;
public:
    typedef std::shared_ptr<Logger> ptr;
    typedef SpinLock MutexType;

    Logger(const std::string& name = "root");

    void log(LogLevel::Level level, LogEvent::ptr event);

    void debug(LogEvent::ptr event);

    void info(LogEvent::ptr event);

    void warn(LogEvent::ptr event);

    void error(LogEvent::ptr event);

    void fatal(LogEvent::ptr event);

    void addAppender(LogAppender::ptr appender);

    void delAppender(LogAppender::ptr appender);

    void clearAppenders();

    LogLevel::Level getLevel() const { return level_;}

    void setLevel(LogLevel::Level val) { level_ = val;}

    const std::string& getName() const { return name_;}

    void setFormatter(LogFormatter::ptr val);

    void setFormatter(const std::string& val);

    LogFormatter::ptr getFormatter();

    std::string toYamlString();
private:
    std::string name_;
    LogLevel::Level level_;
    MutexType mutex_;
    std::list<LogAppender::ptr> appenders_;
    LogFormatter::ptr formatter_;
    Logger::ptr root_;
};

class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;
};

class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string& filename);
    void log(Logger::ptr logger, LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;

    bool reopen();
private:
    std::string filename_;
    std::ofstream filestream_;
    uint64_t lastTime_ = 0;
};

class LoggerManager {
public:
    typedef SpinLock MutexType;

    LoggerManager();

    Logger::ptr getLogger(const std::string& name);

    void init();

    Logger::ptr getRoot() const { return root_;}

    std::string toYamlString();
private:
    MutexType mutex_;
    std::map<std::string, Logger::ptr> logger_;
    Logger::ptr root_;
};

typedef fylee::Singleton<LoggerManager> LoggerMgr;

}

#endif
