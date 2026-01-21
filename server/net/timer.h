#ifndef SERVER_NET_TIMER_H
#define SERVER_NET_TIMER_H

#include "net/callbacks.h"
#include "net/timestamp.h"

#include <atomic>

namespace net {

class Timer {
public:
    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_(std::move(cb)),
          expiration_(when),
          interval_(interval),
          repeat_(interval > 0.0),
          sequence_(s_numCreated_.fetch_add(1)) {}

    void run() const {
        if (callback_) {
            callback_();
        }
    }

    Timestamp expiration() const { return expiration_; }
    bool repeat() const { return repeat_; }
    int64_t sequence() const { return sequence_; }

    void restart(Timestamp now);

    static int64_t numCreated() { return s_numCreated_.load(); }

private:
    const TimerCallback callback_;
    Timestamp expiration_;
    const double interval_;
    const bool repeat_;
    const int64_t sequence_;

    static std::atomic<int64_t> s_numCreated_;
};

} // namespace net

#endif // SERVER_NET_TIMER_H
