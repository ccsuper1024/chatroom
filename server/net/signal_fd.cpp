#include "net/signal_fd.h"
#include "net/event_loop.h"
#include "logger.h"
#include <unistd.h>
#include <cstring>

SignalFd::SignalFd(EventLoop* loop)
    : loop_(loop), fd_(-1) {
    sigemptyset(&mask_);
}

SignalFd::~SignalFd() {
    if (channel_) {
        channel_->disableAll();
        channel_->remove();
    }
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void SignalFd::addSignal(int signo) {
    sigaddset(&mask_, signo);
    
    // Block the signal so it can be handled by signalfd
    if (::sigprocmask(SIG_BLOCK, &mask_, nullptr) == -1) {
        LOG_ERROR("sigprocmask failed");
        return;
    }

    if (fd_ >= 0) {
        ::close(fd_);
        if (channel_) {
            channel_->disableAll();
            channel_->remove();
        }
    }

    fd_ = ::signalfd(-1, &mask_, SFD_NONBLOCK | SFD_CLOEXEC);
    if (fd_ < 0) {
        LOG_ERROR("signalfd failed");
        return;
    }

    channel_ = std::make_unique<Channel>(loop_, fd_);
    channel_->setReadCallback([this]() { handleRead(); });
    channel_->enableReading();
}

void SignalFd::handleRead() {
    struct signalfd_siginfo fdsi;
    ssize_t s = ::read(fd_, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
        LOG_ERROR("SignalFd::handleRead read wrong size");
        return;
    }

    if (callback_) {
        callback_(fdsi.ssi_signo);
    }
}
