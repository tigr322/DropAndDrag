#include "search_engine.hpp"

#include <sqlite3.h>

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

Item item_from_search_row(sqlite3_stmt* stmt) {
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
    item.metadata.created_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(sqlite3_column_int64(stmt, 10)));
    item.metadata.modified_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(sqlite3_column_int64(stmt, 11)));
    item.metadata.accessed_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(sqlite3_column_int64(stmt, 12)));
    item.metadata.is_favorite = sqlite3_column_int(stmt, 13) != 0;

    const char* collection_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
    if (collection_id) item.metadata.collection_id = collection_id;

    return item;
}

} // anonymous namespace

SearchEngine& SearchEngine::instance() {
    static SearchEngine engine;
    return engine;
}

std::vector<Item> SearchEngine::search(std::string_view query) {
    if (!db_) return {};

    std::lock_guard lock(mutex_);

    std::vector<Item> results;

    constexpr std::string_view fts_sql =
        "SELECT items.uuid, items.type, items.path, items.file_name, items.text_content, "
        "items.url, items.title, items.file_size, items.mime_type, items.icon_path, "
        "items.created_at, items.modified_at, items.accessed_at, items.is_favorite, "
        "items.collection_id "
        "FROM search_index JOIN items ON search_index.uuid = items.uuid "
        "WHERE search_index MATCH ? ORDER BY rank LIMIT 50;";

    auto stmt = prepare(db_, fts_sql);
    if (!stmt) {
        constexpr std::string_view fallback_sql =
            "SELECT uuid, type, path, file_name, text_content, url, title, file_size, "
            "mime_type, icon_path, created_at, modified_at, accessed_at, is_favorite, "
            "collection_id FROM items WHERE title LIKE ?1 OR file_name LIKE ?1 LIMIT 50;";

        stmt = prepare(db_, fallback_sql);
        if (!stmt) return {};

        std::string pattern = std::string(query) + "%";
        sqlite3_bind_text(stmt.get(), 1, pattern.c_str(), static_cast<int>(pattern.size()), SQLITE_TRANSIENT);
    } else {
        std::string fts_query = std::string(query) + "*";
        sqlite3_bind_text(stmt.get(), 1, fts_query.c_str(), static_cast<int>(fts_query.size()), SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        results.push_back(item_from_search_row(stmt.get()));
    }

    return results;
}

std::vector<Item> SearchEngine::searchByType(ItemType type) {
    if (!db_) return {};

    std::lock_guard lock(mutex_);

    constexpr std::string_view sql =
        "SELECT uuid, type, path, file_name, text_content, url, title, file_size, "
        "mime_type, icon_path, created_at, modified_at, accessed_at, is_favorite, "
        "collection_id FROM items WHERE type = ? ORDER BY modified_at DESC LIMIT 100;";

    auto stmt = prepare(db_, sql);
    if (!stmt) return {};

    sqlite3_bind_int(stmt.get(), 1, static_cast<int>(type));

    std::vector<Item> results;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        results.push_back(item_from_search_row(stmt.get()));
    }

    return results;
}

std::vector<Item> SearchEngine::searchInCollection(std::string_view collection_id, std::string_view query) {
    if (!db_) return {};

    std::lock_guard lock(mutex_);
    std::string cid(collection_id);

    std::vector<Item> results;
    std::string pattern = "%" + std::string(query) + "%";

    constexpr std::string_view sql =
        "SELECT uuid, type, path, file_name, text_content, url, title, file_size, "
        "mime_type, icon_path, created_at, modified_at, accessed_at, is_favorite, "
        "collection_id FROM items "
        "WHERE collection_id = ? AND (title LIKE ? OR file_name LIKE ?) "
        "ORDER BY modified_at DESC LIMIT 50;";

    auto stmt = prepare(db_, sql);
    if (!stmt) return {};

    sqlite3_bind_text(stmt.get(), 1, cid.c_str(), static_cast<int>(cid.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, pattern.c_str(), static_cast<int>(pattern.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 3, pattern.c_str(), static_cast<int>(pattern.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        results.push_back(item_from_search_row(stmt.get()));
    }

    return results;
}

void SearchEngine::indexItem(const Item& item) {
    if (!db_) return;

    std::lock_guard lock(mutex_);

    constexpr std::string_view sql =
        "INSERT OR REPLACE INTO search_index(rowid, uuid, title, file_name, text_content, url) "
        "VALUES ((SELECT rowid FROM items WHERE uuid = ?), ?, ?, ?, ?, ?);";

    auto stmt = prepare(db_, sql);
    if (!stmt) return;

    sqlite3_bind_text(stmt.get(), 1, item.data.uuid.c_str(), static_cast<int>(item.data.uuid.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, item.data.uuid.c_str(), static_cast<int>(item.data.uuid.size()), SQLITE_TRANSIENT);

    if (item.data.title) sqlite3_bind_text(stmt.get(), 3, item.data.title->c_str(), static_cast<int>(item.data.title->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt.get(), 3);

    if (item.data.file_name) sqlite3_bind_text(stmt.get(), 4, item.data.file_name->c_str(), static_cast<int>(item.data.file_name->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt.get(), 4);

    if (item.data.text_content) sqlite3_bind_text(stmt.get(), 5, item.data.text_content->c_str(), static_cast<int>(item.data.text_content->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt.get(), 5);

    if (item.data.url) sqlite3_bind_text(stmt.get(), 6, item.data.url->c_str(), static_cast<int>(item.data.url->size()), SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt.get(), 6);

    sqlite3_step(stmt.get());
}

void SearchEngine::deindexItem(std::string_view uuid) {
    if (!db_) return;

    std::lock_guard lock(mutex_);

    constexpr std::string_view sql =
        "INSERT INTO search_index(search_index, rowid, uuid, title, file_name, text_content, url) "
        "VALUES ('delete', (SELECT rowid FROM items WHERE uuid = ?), ?, NULL, NULL, NULL, NULL);";

    auto stmt = prepare(db_, sql);
    if (!stmt) return;

    sqlite3_bind_text(stmt.get(), 1, uuid.data(), static_cast<int>(uuid.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, uuid.data(), static_cast<int>(uuid.size()), SQLITE_TRANSIENT);

    sqlite3_step(stmt.get());
}

} // namespace dd
