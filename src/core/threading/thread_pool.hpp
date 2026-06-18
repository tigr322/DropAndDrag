#pragma once

// thread_pool.hpp — Simple fixed-size worker thread pool.
//
// Workers sleep on condition_variable_any::wait(stop_token) so they consume
// zero CPU while the queue is empty.  Tasks are std::function<void()> stored
// in a FIFO queue; there is no priority or work-stealing.
//
// NOTE: The pool is constructed with 0 threads in production (ThreadPool(0))
// because thumbnail work is dispatched through GCD and no other code enqueues
// tasks yet.  The pool is kept around so future features can use enqueue()
// without changing Application::init_threading().
//
// Thread safety: enqueue() is safe from any thread.  shutdown() is
// idempotent and safe to call multiple times.

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
    // Construct a pool with num_threads worker threads.
    // Passing 0 is valid — creates the queue machinery but no workers.
    // Default (hardware_concurrency) is NOT used in production; see NOTE above.
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();   // calls shutdown()

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    // Submit a callable to the pool.  Returns a future for the result.
    // If shutdown() has already been called, the task is executed inline on
    // the calling thread so the future is always valid.
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

    // Request all workers to stop after draining their current task.
    // Subsequent enqueue() calls execute inline (see above).
    void shutdown();

    // Block until all currently queued tasks have completed.
    void wait_all();

    // Accessors — useful for diagnostics.
    [[nodiscard]] size_t worker_count()  const noexcept { return workers_.size(); }
    [[nodiscard]] size_t pending_tasks() const noexcept;

private:
    // Main loop for each worker thread.  Sleeps until a task is available or
    // stop is requested.
    void worker_loop(std::stop_token stoken, size_t index);

    std::vector<std::jthread>          workers_;       // jthread = auto-join on destruction
    std::queue<std::function<void()>>  tasks_;         // pending work items
    mutable std::mutex                 mutex_;         // guards tasks_
    std::condition_variable_any        cv_;            // wakes workers; accepts stop_token
    std::atomic<bool>                  stopped_{false};
    std::atomic<size_t>                pending_count_{0};
};

// --- Template implementation ---

template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using return_type = std::invoke_result_t<F, Args...>;

    // Wrap the callable in a packaged_task so we can extract a future from it.
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind_front(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();

    if (stopped_.load(std::memory_order_acquire)) {
        // Pool is shut down — run inline so the caller's future resolves.
        task->operator()();
        return result;
    }

    {
        std::lock_guard lock(mutex_);
        tasks_.emplace([task = std::move(task)]() mutable { (*task)(); });
        ++pending_count_;
    }

    cv_.notify_one();   // wake one sleeping worker
    return result;
}

} // namespace dd
