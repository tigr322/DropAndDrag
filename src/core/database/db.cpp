#include "db.hpp"
#include "migrations.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstring>

namespace dd {

namespace {

struct StmtDeleter {
    void operator()(sqlite3_stmt* s) const noexcept { if (s) sqlite3_finalize(s); }
};

using StmtPtr = std::unique_ptr<sqlite3_stmt, StmtDeleter>;

StmtPtr prepare(sqlite3* db, std::string_view sql) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v3(db, sql.data(), static_cast<int>(sql.size()), 0, &stmt, nullptr);
    return StmtPtr(stmt);
}

std::string uuid_to_hex(std::string_view uuid) {
    return std::string(uuid);
}

int64_t time_to_epoch(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point epoch_to_time(int64_t epoch) {
    return std::chrono::system_clock::time_point(std::chrono::seconds(epoch));
}

Item item_from_row(sqlite3_stmt* stmt) {
    Item item;
    item.data.uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    item.data.type = static_cast<ItemType>(sqlite3_column_int(stmt, 1));

    const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    if (path) item.data.path = path;

    const char* file_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    if (file_name) item.data.file_name = file_name;

    const char* text_content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    if (text_content) item.data.text_content = text_content;

    const char* url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    if (url) item.data.url = url;

    const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    if (title) item.data.title = title;

    int64_t file_size = sqlite3_column_int64(stmt, 7);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) item.data.file_size = static_cast<uint64_t>(file_size);

    const char* mime_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    if (mime_type) item.data.mime_type = mime_type;

    const char* icon_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
    if (icon_path) item.data.icon_path = icon_path;

    item.metadata.uuid = item.data.uuid;
    item.metadata.created_at = epoch_to_time(sqlite3_column_int64(stmt, 10));
    item.metadata.modified_at = epoch_to_time(sqlite3_column_int64(stmt, 11));
    item.metadata.accessed_at = epoch_to_time(sqlite3_column_int64(stmt, 12));
    item.metadata.is_favorite = sqlite3_column_int(stmt, 13) != 0;

    const char* collection_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
    if (collection_id) item.metadata.collection_id = collection_id;

    return item;
}

void bind_item_params(sqlite3_stmt* stmt, const Item& item, int base_index = 1) {
    sqlite3_bind_text(stmt, base_index,     item.data.uuid.c_str(), static_cast<int>(item.data.uuid.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  base_index + 1, static_cast<int>(item.data.type));

    if (item.data.path) sqlite3_bind_text(stmt, base_index + 2, item.data.path->c_str(), static_cast<int>(item.data.path->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, base_index + 2);

    if (item.data.file_name) sqlite3_bind_text(stmt, base_index + 3, item.data.file_name->c_str(), static_cast<int>(item.data.file_name->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, base_index + 3);

    if (item.data.text_content) sqlite3_bind_text(stmt, base_index + 4, item.data.text_content->c_str(), static_cast<int>(item.data.text_content->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, base_index + 4);

    if (item.data.url) sqlite3_bind_text(stmt, base_index + 5, item.data.url->c_str(), static_cast<int>(item.data.url->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, base_index + 5);

    if (item.data.title) sqlite3_bind_text(stmt, base_index + 6, item.data.title->c_str(), static_cast<int>(item.data.title->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, base_index + 6);

    if (item.data.file_size) sqlite3_bind_int64(stmt, base_index + 7, static_cast<sqlite3_int64>(*item.data.file_size));
    else sqlite3_bind_null(stmt, base_index + 7);

    if (item.data.mime_type) sqlite3_bind_text(stmt, base_index + 8, item.data.mime_type->c_str(), static_cast<int>(item.data.mime_type->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, base_index + 8);

    if (item.data.icon_path) sqlite3_bind_text(stmt, base_index + 9, item.data.icon_path->c_str(), static_cast<int>(item.data.icon_path->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, base_index + 9);

    sqlite3_bind_int64(stmt, base_index + 10, time_to_epoch(item.metadata.created_at));
    sqlite3_bind_int64(stmt, base_index + 11, time_to_epoch(item.metadata.modified_at));
    sqlite3_bind_int64(stmt, base_index + 12, time_to_epoch(item.metadata.accessed_at));
    sqlite3_bind_int(stmt,  base_index + 13, item.metadata.is_favorite ? 1 : 0);

    if (item.metadata.collection_id) sqlite3_bind_text(stmt, base_index + 14, item.metadata.collection_id->c_str(), static_cast<int>(item.metadata.collection_id->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, base_index + 14);
}

void exec_sql(sqlite3* db, std::string_view sql) {
    char* err = nullptr;
    sqlite3_exec(db, sql.data(), nullptr, nullptr, &err);
    if (err) {
        sqlite3_free(err);
    }
}

} // anonymous namespace

Database& Database::instance() {
    static Database db;
    return db;
}

Database::~Database() {
    if (running_.load(std::memory_order_acquire)) {
        close().wait();
    }
    if (db_thread_.joinable()) {
        db_thread_.join();
    }
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

void Database::execute_async(std::move_only_function<void()> task) {
    if (!running_.load(std::memory_order_acquire)) return;

    {
        std::lock_guard lock(queue_mutex_);
        task_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

void Database::db_thread_loop(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        std::move_only_function<void()> task;

        {
            std::unique_lock lock(queue_mutex_);
            queue_cv_.wait(lock, stoken, [this, &stoken] {
                return !task_queue_.empty() || stoken.stop_requested();
            });

            if (stoken.stop_requested()) return;
            if (task_queue_.empty()) continue;

            task = std::move(task_queue_.front());
            task_queue_.pop();
        }

        task();
    }
}

std::future<bool> Database::init(std::string_view db_path) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();

    std::thread([this, promise = std::move(promise), path = std::string(db_path)]() mutable {
        int rc = sqlite3_open_v2(path.c_str(), &db_,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);

        if (rc != SQLITE_OK) {
            promise->set_value(false);
            return;
        }

        exec_sql(db_, "PRAGMA journal_mode=WAL;");
        exec_sql(db_, "PRAGMA synchronous=NORMAL;");
        exec_sql(db_, "PRAGMA cache_size=-8000;");
        exec_sql(db_, "PRAGMA busy_timeout=5000;");
        exec_sql(db_, "PRAGMA foreign_keys=ON;");

        run_migrations(db_);

        running_.store(true, std::memory_order_release);
        db_thread_ = std::jthread([this](std::stop_token stoken) {
            db_thread_loop(std::move(stoken));
        });

        promise->set_value(true);
    }).detach();

    return future;
}

std::future<void> Database::close() {
    running_.store(false, std::memory_order_release);

    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    {
        std::lock_guard lock(queue_mutex_);
        task_queue_.push([this, promise = std::move(promise)]() mutable {
            if (db_thread_.joinable()) {
                db_thread_.request_stop();
            }
            promise->set_value();
        });
    }
    queue_cv_.notify_all();

    return future;
}

std::future<bool> Database::insertItem(const Item& item) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    Item copy = item;

    execute_async([this, promise = std::move(promise), item = std::move(copy)]() mutable {
        constexpr std::string_view sql =
            "INSERT OR REPLACE INTO items (uuid, type, path, file_name, text_content, url, "
            "title, file_size, mime_type, icon_path, created_at, modified_at, accessed_at, "
            "is_favorite, collection_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value(false); return; }

        bind_item_params(stmt.get(), item);

        int rc = sqlite3_step(stmt.get());
        promise->set_value(rc == SQLITE_DONE);
    });

    return future;
}

std::future<bool> Database::updateItem(const Item& item) {
    return insertItem(item);
}

std::future<bool> Database::deleteItem(std::string_view uuid) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    std::string id(uuid);

    execute_async([this, promise = std::move(promise), id = std::move(id)]() mutable {
        constexpr std::string_view sql = "DELETE FROM items WHERE uuid = ?;";
        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value(false); return; }

        sqlite3_bind_text(stmt.get(), 1, id.c_str(), static_cast<int>(id.size()), SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt.get());
        promise->set_value(rc == SQLITE_DONE);
    });

    return future;
}

std::future<std::optional<Item>> Database::getItem(std::string_view uuid) {
    auto promise = std::make_shared<std::promise<std::optional<Item>>>();
    auto future = promise->get_future();
    std::string id(uuid);

    execute_async([this, promise = std::move(promise), id = std::move(id)]() mutable {
        constexpr std::string_view sql =
            "SELECT uuid, type, path, file_name, text_content, url, title, file_size, "
            "mime_type, icon_path, created_at, modified_at, accessed_at, is_favorite, "
            "collection_id FROM items WHERE uuid = ?;";

        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value(std::nullopt); return; }

        sqlite3_bind_text(stmt.get(), 1, id.c_str(), static_cast<int>(id.size()), SQLITE_TRANSIENT);

        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            promise->set_value(item_from_row(stmt.get()));
        } else {
            promise->set_value(std::nullopt);
        }
    });

    return future;
}

std::future<std::vector<Item>> Database::getAllItems() {
    auto promise = std::make_shared<std::promise<std::vector<Item>>>();
    auto future = promise->get_future();

    execute_async([this, promise = std::move(promise)]() mutable {
        constexpr std::string_view sql =
            "SELECT uuid, type, path, file_name, text_content, url, title, file_size, "
            "mime_type, icon_path, created_at, modified_at, accessed_at, is_favorite, "
            "collection_id FROM items ORDER BY modified_at DESC;";

        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value({}); return; }

        std::vector<Item> results;
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            results.push_back(item_from_row(stmt.get()));
        }
        promise->set_value(std::move(results));
    });

    return future;
}

std::future<std::vector<Item>> Database::searchItems(std::string_view query) {
    auto promise = std::make_shared<std::promise<std::vector<Item>>>();
    auto future = promise->get_future();
    std::string q(query);

    execute_async([this, promise = std::move(promise), q = std::move(q)]() mutable {
        constexpr std::string_view sql =
            "SELECT uuid, type, path, file_name, text_content, url, title, file_size, "
            "mime_type, icon_path, created_at, modified_at, accessed_at, is_favorite, "
            "collection_id FROM items WHERE title LIKE ? OR file_name LIKE ? OR text_content LIKE ? "
            "ORDER BY modified_at DESC LIMIT 50;";

        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value({}); return; }

        std::string pattern = "%" + q + "%";
        sqlite3_bind_text(stmt.get(), 1, pattern.c_str(), static_cast<int>(pattern.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, pattern.c_str(), static_cast<int>(pattern.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, pattern.c_str(), static_cast<int>(pattern.size()), SQLITE_TRANSIENT);

        std::vector<Item> results;
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            results.push_back(item_from_row(stmt.get()));
        }
        promise->set_value(std::move(results));
    });

    return future;
}

std::future<bool> Database::insertCollection(std::string_view id, std::string_view name,
                                              std::string_view color, std::string_view icon, int order_index) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    std::string cid(id), cname(name), ccolor(color), cicon(icon);

    execute_async([this, promise = std::move(promise), cid = std::move(cid),
                   cname = std::move(cname), ccolor = std::move(ccolor),
                   cicon = std::move(cicon), order_index]() mutable {
        constexpr std::string_view sql =
            "INSERT OR REPLACE INTO collections (id, name, color, icon, order_index) VALUES (?,?,?,?,?);";

        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value(false); return; }

        sqlite3_bind_text(stmt.get(), 1, cid.c_str(), static_cast<int>(cid.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, cname.c_str(), static_cast<int>(cname.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, ccolor.c_str(), static_cast<int>(ccolor.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 4, cicon.c_str(), static_cast<int>(cicon.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt.get(), 5, order_index);

        int rc = sqlite3_step(stmt.get());
        promise->set_value(rc == SQLITE_DONE);
    });

    return future;
}

std::future<bool> Database::updateCollection(std::string_view id, std::string_view name,
                                              std::string_view color, std::string_view icon, int order_index) {
    return insertCollection(id, name, color, icon, order_index);
}

std::future<bool> Database::deleteCollection(std::string_view id) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    std::string cid(id);

    execute_async([this, promise = std::move(promise), cid = std::move(cid)]() mutable {
        constexpr std::string_view sql = "DELETE FROM collections WHERE id = ?;";
        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value(false); return; }

        sqlite3_bind_text(stmt.get(), 1, cid.c_str(), static_cast<int>(cid.size()), SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt.get());
        promise->set_value(rc == SQLITE_DONE);
    });

    return future;
}

std::future<std::vector<nlohmann::json>> Database::getCollections() {
    auto promise = std::make_shared<std::promise<std::vector<nlohmann::json>>>();
    auto future = promise->get_future();

    execute_async([this, promise = std::move(promise)]() mutable {
        constexpr std::string_view sql =
            "SELECT id, name, color, icon, order_index FROM collections ORDER BY order_index;";

        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value({}); return; }

        std::vector<nlohmann::json> results;
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            nlohmann::json j;
            j["id"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
            j["name"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
            j["color"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
            j["icon"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
            j["order_index"] = sqlite3_column_int(stmt.get(), 4);
            results.push_back(std::move(j));
        }
        promise->set_value(std::move(results));
    });

    return future;
}

std::future<bool> Database::addTag(std::string_view name, std::string_view color) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    std::string tname(name), tcolor(color);

    execute_async([this, promise = std::move(promise), tname = std::move(tname),
                   tcolor = std::move(tcolor)]() mutable {
        constexpr std::string_view sql = "INSERT OR REPLACE INTO tags (name, color) VALUES (?,?);";

        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value(false); return; }

        sqlite3_bind_text(stmt.get(), 1, tname.c_str(), static_cast<int>(tname.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, tcolor.c_str(), static_cast<int>(tcolor.size()), SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt.get());
        promise->set_value(rc == SQLITE_DONE);
    });

    return future;
}

std::future<bool> Database::removeTag(std::string_view name) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    std::string tname(name);

    execute_async([this, promise = std::move(promise), tname = std::move(tname)]() mutable {
        constexpr std::string_view sql = "DELETE FROM tags WHERE name = ?;";
        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value(false); return; }

        sqlite3_bind_text(stmt.get(), 1, tname.c_str(), static_cast<int>(tname.size()), SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt.get());
        promise->set_value(rc == SQLITE_DONE);
    });

    return future;
}

std::future<std::vector<nlohmann::json>> Database::getTags() {
    auto promise = std::make_shared<std::promise<std::vector<nlohmann::json>>>();
    auto future = promise->get_future();

    execute_async([this, promise = std::move(promise)]() mutable {
        constexpr std::string_view sql = "SELECT name, color FROM tags ORDER BY name;";

        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value({}); return; }

        std::vector<nlohmann::json> results;
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            nlohmann::json j;
            j["name"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
            j["color"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
            results.push_back(std::move(j));
        }
        promise->set_value(std::move(results));
    });

    return future;
}

std::future<bool> Database::setFavorite(std::string_view uuid, bool favorite) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    std::string id(uuid);

    execute_async([this, promise = std::move(promise), id = std::move(id), favorite]() mutable {
        constexpr std::string_view sql = "UPDATE items SET is_favorite = ? WHERE uuid = ?;";
        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value(false); return; }

        sqlite3_bind_int(stmt.get(), 1, favorite ? 1 : 0);
        sqlite3_bind_text(stmt.get(), 2, id.c_str(), static_cast<int>(id.size()), SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt.get());
        promise->set_value(rc == SQLITE_DONE);
    });

    return future;
}

std::future<std::vector<Item>> Database::getFavorites() {
    auto promise = std::make_shared<std::promise<std::vector<Item>>>();
    auto future = promise->get_future();

    execute_async([this, promise = std::move(promise)]() mutable {
        constexpr std::string_view sql =
            "SELECT uuid, type, path, file_name, text_content, url, title, file_size, "
            "mime_type, icon_path, created_at, modified_at, accessed_at, is_favorite, "
            "collection_id FROM items WHERE is_favorite = 1 ORDER BY modified_at DESC;";

        auto stmt = prepare(db_, sql);
        if (!stmt) { promise->set_value({}); return; }

        std::vector<Item> results;
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            results.push_back(item_from_row(stmt.get()));
        }
        promise->set_value(std::move(results));
    });

    return future;
}

} // namespace dd
