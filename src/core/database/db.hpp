#pragma once

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

struct sqlite3;

namespace dd {

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

    std::future<bool> init(std::string_view db_path);
    std::future<void> close();

    std::future<bool>                insertItem(const Item& item);
    std::future<bool>                updateItem(const Item& item);
    std::future<bool>                deleteItem(std::string_view uuid);
    std::future<std::optional<Item>> getItem(std::string_view uuid);
    std::future<std::vector<Item>>   getAllItems();
    std::future<std::vector<Item>>   searchItems(std::string_view query);

    std::future<bool> insertCollection(std::string_view id, std::string_view name,
                                       std::string_view color, std::string_view icon,
                                       int order_index);
    std::future<bool> updateCollection(std::string_view id, std::string_view name,
                                       std::string_view color, std::string_view icon,
                                       int order_index);
    std::future<bool>                    deleteCollection(std::string_view id);
    std::future<std::vector<Collection>> getCollections();

    std::future<bool>             addTag(std::string_view name, std::string_view color);
    std::future<bool>             removeTag(std::string_view name);
    std::future<std::vector<Tag>> getTags();

    std::future<bool>              setFavorite(std::string_view uuid, bool favorite);
    std::future<std::vector<Item>> getFavorites();

    [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }

private:
    void db_thread_loop(std::stop_token stoken);
    void execute_async(std::function<void()> task);

    sqlite3* db_{nullptr};
    std::jthread db_thread_;
    std::queue<std::function<void()>> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable_any queue_cv_;
    std::atomic<bool> running_{false};
};

} // namespace dd
