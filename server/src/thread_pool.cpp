#include "thread_pool.h"

ThreadPool::ThreadPool(std::size_t core_threads,
                       std::size_t max_threads,
                       std::size_t queue_capacity)
    : core_threads_(core_threads),
      max_threads_(max_threads),
      queue_capacity_(queue_capacity),
      stop_(false),
      current_threads_(0),
      rejected_tasks_(0) {
    if (core_threads_ == 0) {
        core_threads_ = 1;
    }
    if (max_threads_ < core_threads_) {
        max_threads_ = core_threads_;
    }
    if (queue_capacity_ == 0) {
        queue_capacity_ = 1024;
    }
    for (std::size_t i = 0; i < core_threads_; ++i) {
        addWorker();
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void ThreadPool::post(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this]() { return stop_ || tasks_.size() < queue_capacity_; });
        if (stop_) {
            return;
        }
        tasks_.push(std::move(task));
        if (tasks_.size() > current_threads_ && current_threads_ < max_threads_) {
            addWorker();
        }
    }
    not_empty_.notify_one();
}

bool ThreadPool::tryPost(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stop_) {
            return false;
        }
        if (tasks_.size() >= queue_capacity_) {
            ++rejected_tasks_;
            return false;
        }
        tasks_.push(std::move(task));
        if (tasks_.size() > current_threads_ && current_threads_ < max_threads_) {
            addWorker();
        }
    }
    not_empty_.notify_one();
    return true;
}

std::size_t ThreadPool::currentThreadCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_threads_;
}

std::size_t ThreadPool::queueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

std::size_t ThreadPool::rejectedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rejected_tasks_;
}

void ThreadPool::addWorker() {
    workers_.emplace_back([this]() { this->workerLoop(); });
    ++current_threads_;
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            not_empty_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
            not_full_.notify_one();
        }
        task();
    }
}

