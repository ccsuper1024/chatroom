#include "net/timer_queue.h"
#include "net/event_loop.h"
#include "net/timer.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace net {

namespace detail {

int createTimerfd() {
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        perror("createTimerfd");
    }
    return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when) {
    int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if (microseconds < 100) {
        microseconds = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>((microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

void readTimerfd(int timerfd) {
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    if (n != sizeof howmany) {
        std::cerr << "TimerQueue::handleRead() reads " << n << " bytes instead of 8" << std::endl;
    }
}

void resetTimerfd(int timerfd, Timestamp expiration) {
    struct itimerspec newValue;
    struct itimerspec oldValue;
    std::memset(&newValue, 0, sizeof newValue);
    std::memset(&oldValue, 0, sizeof oldValue);
    
    newValue.it_value = howMuchTimeFromNow(expiration);
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret) {
        perror("timerfd_settime");
    }
}

} // namespace detail

using namespace detail;

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop, timerfd_),
      timers_() {
    timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue() {
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
    // Clean up all timers
    for (const auto& entry : timers_) {
        delete entry.second;
    }
}

void TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval) {
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop([this, timer]() {
        bool earliestChanged = insert(timer);
        if (earliestChanged) {
            resetTimerfd(timerfd_, timer->expiration());
        }
    });
}

void TimerQueue::handleRead() {
    loop_->assertInLoopThread();
    Timestamp now = Timestamp::now();
    readTimerfd(timerfd_);

    std::vector<Entry> expired = getExpired(now);

    // callback
    for (const auto& entry : expired) {
        entry.second->run();
    }

    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now) {
    std::vector<Entry> expired;
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    TimerList::iterator end = timers_.lower_bound(sentry);
    std::copy(timers_.begin(), end, std::back_inserter(expired));
    timers_.erase(timers_.begin(), end);
    return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now) {
    Timestamp nextExpire;

    for (const auto& entry : expired) {
        Timer* timer = entry.second;
        if (timer->repeat()) {
            timer->restart(now);
            insert(timer);
        } else {
            delete timer;
        }
    }

    if (!timers_.empty()) {
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid()) {
        resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(Timer* timer) {
    loop_->assertInLoopThread();
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first) {
        earliestChanged = true;
    }
    
    timers_.insert(Entry(when, timer));
    return earliestChanged;
}

} // namespace net
