#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <vector>

class ThreadPool {
public:
    ThreadPool(std::size_t core_threads,
               std::size_t max_threads,
               std::size_t queue_capacity);
    ~ThreadPool();

    void post(std::function<void()> task);
    bool tryPost(std::function<void()> task);

    std::size_t currentThreadCount() const;
    std::size_t queueSize() const;
    std::size_t rejectedCount() const;

private:
    void addWorker();
    void workerLoop();

    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::size_t core_threads_;
    std::size_t max_threads_;
    std::size_t queue_capacity_;
    bool stop_;
    std::size_t current_threads_;
    std::size_t rejected_tasks_;
};

