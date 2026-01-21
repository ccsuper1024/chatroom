#ifndef SERVER_NET_TIMER_QUEUE_H
#define SERVER_NET_TIMER_QUEUE_H

#include <set>
#include <vector>
#include <memory>

#include "net/timestamp.h"
#include "net/callbacks.h"
#include "net/channel.h"

class EventLoop;

namespace net {

class Timer;

class TimerQueue {
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // Schedules the callback to be run at given time,
    // repeats if interval > 0.0.
    // Returns a Timer* which can be used to cancel (TODO: implement TimerId)
    void addTimer(TimerCallback cb, Timestamp when, double interval);

private:
    using Entry = std::pair<Timestamp, Timer*>;
    using TimerList = std::set<Entry>;

    void handleRead();
    // Move out all expired timers
    std::vector<Entry> getExpired(Timestamp now);
    void reset(const std::vector<Entry>& expired, Timestamp now);

    bool insert(Timer* timer);

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfdChannel_;
    // Timer list sorted by expiration
    TimerList timers_;
};

} // namespace net

#endif // SERVER_NET_TIMER_QUEUE_H
