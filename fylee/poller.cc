#include "poller.h"
#include "macro.h"
#include "channel.h"
#include "log.h"

namespace fylee {
static fylee::Logger::ptr g_logger = LOG_NAME("system");

template<typename To, typename From>
inline To implicit_cast(From const &f) {
    return f;
}

bool Poller::hasChannel(Channel* channel) const {
    assertInLoopThread();
    auto it = channels_.find(channel->getFd());
    return it != channels_.end() && it->second == channel;
}

Poller::Poller(EventLoop* loop) 
    :ownerLoop_(loop),
     epollfd_(epoll_create(8192)),
     events_(kInitEventListSize) {
    if (epollfd_ < 0) {
        LOG_FATAL(g_logger) << "Create poll fd error.";
    }
}

Poller::~Poller() {
    ::close(epollfd_);
}

uint64_t Poller::poll(int timeoutMs, ChannelList* activeChannels) {
    LOG_INFO(g_logger) << "fd total count " << channels_.size();
    int numEvents = ::epoll_wait(epollfd_,
                                &*events_.begin(),
                                static_cast<int>(events_.size()),
                                timeoutMs);
    int savedErrno = errno;
    uint64_t now_ms = fylee::GetCurrentMS();
    if (numEvents > 0) {
        LOG_INFO(g_logger) << numEvents << " events happened";
        fillActiveChannels(numEvents, activeChannels);
        if (implicit_cast<size_t>(numEvents) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {
        LOG_INFO(g_logger) << "nothing happened";
    } else {
        // error happens, log uncommon ones
        if (savedErrno != EINTR) {
            errno = savedErrno;
            LOG_ERROR(g_logger) << "Poller::poll()";
        }
    }
    return now_ms;
}

void Poller::fillActiveChannels(int numEvents, 
                                ChannelList* activeChannels) const {
    ASSERT(implicit_cast<size_t>(numEvents) <= events_.size());
    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
    #ifndef NDEBUG
        int fd = channel->getFd();
        auto it = channels_.find(fd);
        ASSERT(it != channels_.end());
        ASSERT(it->second == channel);
    #endif
        channel->setRevents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

void Poller::updateChannel(Channel* channel) {
    assertInLoopThread();
    const int index = channel->getIndex();
    LOG_INFO(g_logger) << "fd = " << channel->getFd() 
                       << " events = " << channel->getEvents() << " index = " << index;
    if (index == kNew || index == kDeleted) {
        // a new one, add with EPOLL_CTL_ADD
        int fd = channel->getFd();
        if (index == kNew) {
            ASSERT(channels_.find(fd) == channels_.end());
            channels_[fd] = channel;
        } else { // index == kDeleted
            ASSERT(channels_.find(fd) != channels_.end());
            ASSERT(channels_[fd] == channel);
        }

        channel->setIndex(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else {
        // update existing one with EPOLL_CTL_MOD/DEL
        int fd = channel->getFd();
        ASSERT(channels_.find(fd) != channels_.end());
        ASSERT(channels_[fd] == channel);
        ASSERT(index == kAdded);
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->setIndex(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void Poller::removeChannel(Channel* channel) {
    assertInLoopThread();
    int fd = channel->getFd();
    LOG_INFO(g_logger) << "fd = " << fd;
    ASSERT(channels_.find(fd) != channels_.end());
    ASSERT(channels_[fd] == channel);
    ASSERT(channel->isNoneEvent());
    int index = channel->getIndex();
    ASSERT(index == kAdded || index == kDeleted);
    size_t n = channels_.erase(fd);
    ASSERT(n == 1);

    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->setIndex(kNew);
}

void Poller::update(int operation, Channel* channel) {
    struct epoll_event event;
    bzero(&event, sizeof(event));
    event.events = channel->getEvents();
    event.data.ptr = channel;
    int fd = channel->getFd();
    LOG_INFO(g_logger) << "epoll_ctl op = " << operationToString(operation)
                << " fd = " << fd << " event = { " << channel->eventsToString() << " }";
    if (epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        LOG_ERROR(g_logger) << "epoll_ctl failed op =" 
                << operationToString(operation) << " fd =" << fd;
    }
}

const char* Poller::operationToString(int op) {
    switch((int)op) {
#define XX(ctl) \
        case ctl: \
            return #ctl;
        XX(EPOLL_CTL_ADD);
        XX(EPOLL_CTL_MOD);
        XX(EPOLL_CTL_DEL);
        default:
            return "Unknown Operation";
    }
#undef XX
}
}