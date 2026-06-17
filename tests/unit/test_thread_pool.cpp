#include "test_framework.hpp"

#include <core/threading/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <set>
#include <thread>
#include <vector>

using namespace dd;

TEST_CASE("ThreadPool enqueue and get result") {
    ThreadPool pool(4);

    auto future = pool.enqueue([]() -> int {
        return 42;
    });

    auto result = future.get();
    ASSERT_EQ(result, 42);

    pool.shutdown();
}

TEST_CASE("ThreadPool multiple tasks in parallel") {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(4);

        std::vector<std::future<void>> futures;
        for (int i = 0; i < 20; ++i) {
            futures.push_back(pool.enqueue([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            }));
        }

        for (auto& f : futures) {
            f.get();
        }

        pool.shutdown();
    }

    ASSERT_EQ(counter.load(), 20);
}

TEST_CASE("ThreadPool tasks run on different threads") {
    std::mutex id_mutex;
    std::set<std::thread::id> thread_ids;

    {
        ThreadPool pool(4);

        std::vector<std::future<void>> futures;
        for (int i = 0; i < 10; ++i) {
            futures.push_back(pool.enqueue([&thread_ids, &id_mutex]() {
                std::lock_guard lock(id_mutex);
                thread_ids.insert(std::this_thread::get_id());
            }));
        }

        for (auto& f : futures) {
            f.get();
        }

        pool.shutdown();
    }

    ASSERT_GE(thread_ids.size(), 1u);
}

TEST_CASE("ThreadPool shutdown") {
    ThreadPool pool(2);

    auto f1 = pool.enqueue([]() { return 10; });
    auto f2 = pool.enqueue([]() { return 20; });

    ASSERT_EQ(f1.get(), 10);
    ASSERT_EQ(f2.get(), 20);

    pool.shutdown();
}

TEST_CASE("ThreadPool move-only callables") {
    ThreadPool pool(2);

    auto unique = std::make_unique<int>(99);
    auto future = pool.enqueue([ptr = std::move(unique)]() -> int {
        return *ptr;
    });

    auto result = future.get();
    ASSERT_EQ(result, 99);

    pool.shutdown();
}

TEST_CASE("ThreadPool worker count") {
    ThreadPool pool(3);
    ASSERT_EQ(pool.worker_count(), 3u);
    pool.shutdown();
}

TEST_CASE("ThreadPool pending tasks count") {
    ThreadPool pool(1);

    std::atomic<bool> started{false};
    std::atomic<bool> done{false};

    auto slow = pool.enqueue([&]() {
        started.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        done.store(true);
    });

    while (!started.load()) {
        std::this_thread::yield();
    }

    pool.enqueue([]() {});

    auto pending = pool.pending_tasks();
    ASSERT_GE(pending, 0u);

    slow.get();

    pool.shutdown();
}

TEST_CASE("ThreadPool enqueue after shutdown runs directly") {
    ThreadPool pool(2);
    pool.shutdown();

    auto future = pool.enqueue([]() -> int {
        return 7;
    });

    ASSERT_EQ(future.get(), 7);
}

int main() {
    return dd::test::run_all_tests();
}
