#include "channel.h"
#include "log.h"
#include "macro.h"
#include "eventloop.h"
namespace fylee {
static fylee::Logger::ptr g_logger = LOG_NAME("system");
Channel::Channel(EventLoop* loop, int fd) 
    :loop_(loop),
     fd_(fd),
     events_(0),
     revents_(0),
     index_(-1),
     logHup_(true),
     tied_(false),
     eventHandling_(false),
     addedToLoop_(false) { }

Channel::~Channel() {
    ASSERT(!eventHandling_);
    ASSERT(!addedToLoop_);
    if (loop_->isInLoopThread()) {
        ASSERT(!loop_->hasChannel(this));
    }
}

void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
}

void Channel::update() {
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

void Channel::remove() {
    ASSERT(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::handleEvent(uint64_t receiveTime) {
    if (tied_) {
        if (tie_.lock()) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(uint64_t receiveTime) {
    eventHandling_ = true;
    LOG_INFO(g_logger) << reventsToString();
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (logHup_) {
            LOG_WARN(g_logger) << "fd = " << fd_ << " Channel::handle_event() POLLHUP";
        }
        if (closeCallback_) {
            closeCallback_();
        }       
    }

    if(revents_ & EPOLLERR) {
        if (errorCallback_) {
            errorCallback_();
        }
    }
    if(revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }   
    }
    // if(!isNoneEvent()) {
    //     if ((events_ & EPOLLIN) && (events_ & EPOLLOUT)) {
    //         events_ = EPOLLOUT;
    //     }
    //     events_ |= EPOLLET;
    // } else {
    //     events_ |= (EPOLLIN | EPOLLET);
    // }
    // update();
    eventHandling_ = false;
}

std::string Channel::reventsToString() const {
    return eventsToString(fd_, (EPOLL_EVENTS)revents_);
}

std::string Channel::eventsToString() const {
  return eventsToString(fd_, (EPOLL_EVENTS)events_);
}

std::string Channel::eventsToString(int fd, EPOLL_EVENTS event) {
    std::ostringstream oss;
    oss << "fd = " << fd << ": ";
#define XX(E) \
    if(event & E) { \
        oss << #E; \
    }
    XX(EPOLLIN);
    XX(EPOLLPRI);
    XX(EPOLLOUT);
    XX(EPOLLRDNORM);
    XX(EPOLLRDBAND);
    XX(EPOLLWRNORM);
    XX(EPOLLWRBAND);
    XX(EPOLLMSG);
    XX(EPOLLERR);
    XX(EPOLLHUP);
    XX(EPOLLRDHUP);
    XX(EPOLLONESHOT);
    XX(EPOLLET);
#undef XX
    return oss.str();
}
}
