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
    if (stopped_.exchange(true, std::memory_order_acq_rel)) return;
    cv_.notify_all();
    for (auto& w : workers_) {
        w.request_stop();
    }
    workers_.clear();
}

void ThreadPool::wait_all() {
    while (pending_tasks() > 0) {
        std::this_thread::yield();
    }
}

size_t ThreadPool::pending_tasks() const noexcept {
    return pending_count_.load(std::memory_order_acquire);
}

void ThreadPool::worker_loop(std::stop_token stoken, size_t /*index*/) {
    while (!stoken.stop_requested()) {
        std::move_only_function<void()> task;

        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, stoken, [this, &stoken] {
                return !tasks_.empty() || stoken.stop_requested();
            });

            if (stoken.stop_requested()) return;
            if (tasks_.empty()) continue;

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        task();
        --pending_count_;
    }
}

} // namespace dd
