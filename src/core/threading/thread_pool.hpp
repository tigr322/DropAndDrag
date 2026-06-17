#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>

namespace dd {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

    void shutdown();
    void wait_all();
    [[nodiscard]] size_t worker_count() const noexcept { return workers_.size(); }
    [[nodiscard]] size_t pending_tasks() const noexcept;

private:
    void worker_loop(std::stop_token stoken, size_t index);

    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable_any cv_;
    std::atomic<bool> stopped_{false};
    std::atomic<size_t> pending_count_{0};
};

template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using return_type = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind_front(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();

    if (stopped_.load(std::memory_order_acquire)) {
        task->operator()();
        return result;
    }

    {
        std::lock_guard lock(mutex_);
        tasks_.emplace([task = std::move(task)]() mutable { (*task)(); });
        ++pending_count_;
    }

    cv_.notify_one();
    return result;
}

} // namespace dd
