#include "net/event_loop.h"
#include "net/channel.h"
#include "net/poller.h"
#include "net/timer_queue.h"
#include "logger.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <algorithm>

namespace {
    __thread EventLoop* t_loopInThisThread = nullptr;

    int createEventfd() {
        int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (evtfd < 0) {
            LOG_FATAL("Failed in eventfd");
        }
        return evtfd;
    }
}

EventLoop* EventLoop::getEventLoopOfCurrentThread() {
    return t_loopInThisThread;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      threadId_(std::this_thread::get_id()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      timerQueue_(new net::TimerQueue(this)) {
    
    if (t_loopInThisThread) {
        LOG_FATAL("Another EventLoop exists in this thread");
    } else {
        t_loopInThisThread = this;
    }

    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    if (looping_) return;
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop {} start looping", (void*)this);

    while (!quit_) {
        activeChannels_.clear();
        poller_->poll(10000, &activeChannels_);
        
        eventHandling_ = true;
        for (Channel* channel : activeChannels_) {
            channel->handleEvent();
        }
        eventHandling_ = false;

        doPendingFunctors();
    }
    LOG_INFO("EventLoop {} stop looping", (void*)this);
    looping_ = false;
}

void EventLoop::stop() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));
    }
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::updateChannel(Channel* channel) {
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel) {
    return poller_->hasChannel(channel);
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::wakeup() writes {} bytes instead of 8", n);
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::handleRead() reads {} bytes instead of 8", n);
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const auto& functor : functors) {
        functor();
    }
    callingPendingFunctors_ = false;
}

void EventLoop::runAt(Timestamp time, TimerCallback cb) {
    timerQueue_->addTimer(std::move(cb), time, 0.0);
}

void EventLoop::runAfter(double delay, TimerCallback cb) {
    Timestamp time = net::addTime(Timestamp::now(), delay);
    runAt(time, std::move(cb));
}

void EventLoop::runEvery(double interval, TimerCallback cb) {
    Timestamp time = net::addTime(Timestamp::now(), interval);
    timerQueue_->addTimer(std::move(cb), time, interval);
}
