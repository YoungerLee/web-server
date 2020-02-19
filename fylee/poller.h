#ifndef __FYLEE_POLLER_H_
#define __FYLEE_POLLER_H_
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <unistd.h>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include "eventloop.h"
#include "noncopyable.h"

namespace fylee {
class Channel;
class Poller : Noncopyable {
public:
    typedef std::shared_ptr<Poller> ptr;
    typedef std::vector<Channel*> ChannelList;

    Poller(EventLoop* loop);
    ~Poller();

    uint64_t poll(int timeoutMs, ChannelList* activeChannels);

    void updateChannel(Channel* channel);

    void removeChannel(Channel* channel);

    bool hasChannel(Channel* channel) const;

    static Poller* newDefaultPoller(EventLoop* loop);

    void assertInLoopThread() const {
        ownerLoop_->assertInLoopThread();
    }

protected:
    typedef std::map<int, Channel*> ChannelMap;
    ChannelMap channels_;

private:
    enum EpollOp {
        kNew = -1,
        kAdded = 1,
        kDeleted = 2,
    };
    typedef std::vector<struct epoll_event> EventList;
    EventLoop* ownerLoop_;
    int epollfd_;
    EventList events_;
    static const int kInitEventListSize = 16;
    static const char* operationToString(int op);
    void fillActiveChannels(int numEvents,
                            ChannelList* activeChannels) const;
    void update(int operation, Channel* channel);
};   
}
#endif