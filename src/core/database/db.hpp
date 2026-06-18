#pragma once

// db.hpp — Async SQLite3 database wrapper.
//
// All operations are dispatched to a dedicated database thread (db_thread_)
// so the main/UI thread is never blocked on I/O.  Each public method returns
// a std::future<T>; the caller can .wait()/.get() when it needs the result.
//
// Threading rules:
//   • sqlite3* is only ever touched on db_thread_.  Never call DB methods
//     from a DB task callback — the queue is single-threaded and will deadlock.
//   • Application owns the Database instance and constructs it directly;
//     there is no singleton for Database.
//
// Migration strategy:
//   init() runs migrate_vN() steps in sequence.  Each migration executes
//   inside BEGIN IMMEDIATE / COMMIT / ROLLBACK so a partial failure leaves
//   the schema version unchanged; the next startup retries from that version.
//
// WAL mode is enabled on open for better concurrent read performance.

#include "../collections/collection.hpp"
#include "../items/item.hpp"
#include "../tags/tag.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

struct sqlite3;   // forward-declare; callers don't need the full SQLite header

namespace dd {

class Database {
public:
    Database() = default;
    ~Database();   // blocks until db_thread_ has flushed its queue and exited

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&)                 = delete;
    Database& operator=(Database&&)      = delete;

    // --- Lifecycle ---

    // Open the database file at db_path, run all pending migrations, and
    // enable WAL mode.  Must be called once before any other method.
    // Returns a future<bool>; true on success.
    std::future<bool> init(std::string_view db_path);

    // Flush the task queue and close the sqlite3 handle.
    // After close() returns, the future is resolved and it is safe to destroy
    // the Database object.
    std::future<void> close();

    // --- Item CRUD ---

    std::future<bool>                insertItem(const Item& item);
    std::future<bool>                updateItem(const Item& item);
    std::future<bool>                deleteItem(std::string_view uuid);
    std::future<std::optional<Item>> getItem(std::string_view uuid);
    std::future<std::vector<Item>>   getAllItems();

    // Full-text search via SQLite FTS5.  Returns matching items ordered by rank.
    std::future<std::vector<Item>>   searchItems(std::string_view query);

    // --- Collection CRUD ---

    std::future<bool> insertCollection(std::string_view id, std::string_view name,
                                       std::string_view color, std::string_view icon,
                                       int order_index);
    std::future<bool> updateCollection(std::string_view id, std::string_view name,
                                       std::string_view color, std::string_view icon,
                                       int order_index);
    std::future<bool>                    deleteCollection(std::string_view id);
    std::future<std::vector<Collection>> getCollections();

    // --- Tag CRUD ---

    std::future<bool>             addTag(std::string_view name, std::string_view color);
    std::future<bool>             removeTag(std::string_view name);
    std::future<std::vector<Tag>> getTags();

    // --- Favourites ---

    std::future<bool>              setFavorite(std::string_view uuid, bool favorite);
    std::future<std::vector<Item>> getFavorites();

    // True once init() has completed successfully and before close() is called.
    [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }

private:
    // The DB thread's main loop: dequeues and executes tasks until stop is requested.
    void db_thread_loop(std::stop_token stoken);

    // Enqueue a task onto the DB thread's message queue.
    // The task will execute on db_thread_ at some future point.
    void execute_async(std::function<void()> task);

    sqlite3*                           db_{nullptr};    // only touched on db_thread_
    std::jthread                       db_thread_;      // dedicated serialisation thread
    std::queue<std::function<void()>>  task_queue_;     // pending DB tasks
    mutable std::mutex                 queue_mutex_;    // guards task_queue_
    std::condition_variable_any        queue_cv_;       // wakes db_thread_ on new tasks
    std::atomic<bool>                  running_{false};
};

} // namespace dd
