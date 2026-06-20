// thread_pool.cpp — std::jthread-based worker pool.
//
// Construction: num_threads workers are started immediately.  Each calls
// worker_loop() which blocks on cv_ until a task is enqueued or stop is
// requested.  Passing 0 creates zero workers — the pool is reserved for future
// use without burning threads (Application does this today).
//
// Shutdown: stopped_ is set first (prevents new enqueue), then all workers are
// notified and request_stop() is sent.  workers_.clear() joins them via
// std::jthread RAII.  Idempotent — safe to call from ~ThreadPool.
//
// pending_count_ is incremented by submit() before the task is enqueued so
// wait_all() is accurate even if a worker hasn't popped the task yet.

#include "thread_pool.hpp"

namespace dd {

ThreadPool::ThreadPool(size_t num_threads) {
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this, i](std::stop_token stoken) {
            worker_loop(std::move(stoken), i);
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    // acq_rel: acquire to see all prior submits, release so workers see stopped_.
    if (stopped_.exchange(true, std::memory_order_acq_rel)) return;
    cv_.notify_all();
    for (auto& w : workers_) {
        w.request_stop();  // sets stop_token; worker_loop checks it on next cv_.wait
    }
    workers_.clear();  // joins each jthread (destructor blocks until thread exits)
}

void ThreadPool::wait_all() {
    size_t cur = pending_count_.load(std::memory_order_acquire);
    while (cur > 0) {
        pending_count_.wait(cur, std::memory_order_acquire);
        cur = pending_count_.load(std::memory_order_acquire);
    }
}

size_t ThreadPool::pending_tasks() const noexcept {
    return pending_count_.load(std::memory_order_acquire);
}

void ThreadPool::worker_loop(std::stop_token stoken, size_t /*index*/) {
    while (!stoken.stop_requested()) {
        std::function<void()> task;

        {
            std::unique_lock lock(mutex_);
            // C++20 stop-token overload: wakes on cv_.notify_all() OR when
            // stoken.stop_requested() becomes true — no spurious wake check needed.
            cv_.wait(lock, stoken, [this, &stoken] {
                return !tasks_.empty() || stoken.stop_requested();
            });

            if (stoken.stop_requested()) return;
            if (tasks_.empty()) continue;

            task = std::move(tasks_.front());
            tasks_.pop();
        }
        // Lock released before running the task — other workers can dequeue in parallel.
        task();
        if (--pending_count_ == 0) pending_count_.notify_all();
    }
}

} // namespace dd
