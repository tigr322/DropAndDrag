#include "migrations.hpp"

#include <sqlite3.h>

#include <string_view>

namespace dd {

namespace {

constexpr int kCurrentVersion = 1;

void exec(sqlite3* db, std::string_view sql) {
    char* err = nullptr;
    sqlite3_exec(db, sql.data(), nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
}

int get_user_version(sqlite3* db) {
    auto stmt_raw = [&]() -> sqlite3_stmt* {
        sqlite3_stmt* s = nullptr;
        sqlite3_prepare_v3(db, "PRAGMA user_version;", -1, 0, &s, nullptr);
        return s;
    }();

    int version = 0;
    if (stmt_raw && sqlite3_step(stmt_raw) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt_raw, 0);
    }
    if (stmt_raw) sqlite3_finalize(stmt_raw);

    return version;
}

void set_user_version(sqlite3* db, int version) {
    std::string sql = "PRAGMA user_version = " + std::to_string(version) + ";";
    exec(db, sql);
}

void migrate_v1(sqlite3* db) {
    exec(db, R"(
        CREATE TABLE IF NOT EXISTS items (
            uuid TEXT PRIMARY KEY,
            type INTEGER NOT NULL DEFAULT 5,
            path TEXT,
            file_name TEXT,
            text_content TEXT,
            url TEXT,
            title TEXT,
            file_size INTEGER,
            mime_type TEXT,
            icon_path TEXT,
            created_at INTEGER NOT NULL,
            modified_at INTEGER NOT NULL,
            accessed_at INTEGER NOT NULL,
            is_favorite INTEGER NOT NULL DEFAULT 0,
            collection_id TEXT,
            FOREIGN KEY (collection_id) REFERENCES collections(id) ON DELETE SET NULL
        );
    )");

    exec(db, R"(
        CREATE TABLE IF NOT EXISTS collections (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            color TEXT NOT NULL DEFAULT '#3B82F6',
            icon TEXT NOT NULL DEFAULT 'folder',
            order_index INTEGER NOT NULL DEFAULT 0,
            created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
        );
    )");

    exec(db, R"(
        CREATE TABLE IF NOT EXISTS tags (
            name TEXT PRIMARY KEY,
            color TEXT NOT NULL DEFAULT '#6B7280'
        );
    )");

    exec(db, R"(
        CREATE TABLE IF NOT EXISTS item_tags (
            item_uuid TEXT NOT NULL,
            tag_name TEXT NOT NULL,
            PRIMARY KEY (item_uuid, tag_name),
            FOREIGN KEY (item_uuid) REFERENCES items(uuid) ON DELETE CASCADE,
            FOREIGN KEY (tag_name) REFERENCES tags(name) ON DELETE CASCADE
        );
    )");

    exec(db, R"(
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )");

    exec(db, R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS search_index USING fts5(
            uuid UNINDEXED,
            title,
            file_name,
            text_content,
            url,
            content='items',
            content_rowid='rowid'
        );
    )");

    exec(db, R"(
        CREATE TRIGGER IF NOT EXISTS items_ai AFTER INSERT ON items BEGIN
            INSERT INTO search_index(rowid, title, file_name, text_content, url)
            VALUES (new.rowid, new.title, new.file_name, new.text_content, new.url);
        END;
    )");

    exec(db, R"(
        CREATE TRIGGER IF NOT EXISTS items_ad AFTER DELETE ON items BEGIN
            INSERT INTO search_index(search_index, rowid, title, file_name, text_content, url)
            VALUES ('delete', old.rowid, old.title, old.file_name, old.text_content, old.url);
        END;
    )");

    exec(db, R"(
        CREATE TRIGGER IF NOT EXISTS items_au AFTER UPDATE ON items BEGIN
            INSERT INTO search_index(search_index, rowid, title, file_name, text_content, url)
            VALUES ('delete', old.rowid, old.title, old.file_name, old.text_content, old.url);
            INSERT INTO search_index(rowid, title, file_name, text_content, url)
            VALUES (new.rowid, new.title, new.file_name, new.text_content, new.url);
        END;
    )");

    exec(db, "CREATE INDEX IF NOT EXISTS idx_items_type ON items(type);");
    exec(db, "CREATE INDEX IF NOT EXISTS idx_items_collection ON items(collection_id);");
    exec(db, "CREATE INDEX IF NOT EXISTS idx_items_favorite ON items(is_favorite);");
    exec(db, "CREATE INDEX IF NOT EXISTS idx_items_modified ON items(modified_at DESC);");
    exec(db, "CREATE INDEX IF NOT EXISTS idx_item_tags_item ON item_tags(item_uuid);");
    exec(db, "CREATE INDEX IF NOT EXISTS idx_item_tags_tag ON item_tags(tag_name);");
}

} // anonymous namespace

void run_migrations(sqlite3* db) {
    int version = get_user_version(db);

    if (version < 1) {
        migrate_v1(db);
        set_user_version(db, 1);
    }

    if (version < kCurrentVersion) {
        set_user_version(db, kCurrentVersion);
    }
}

} // namespace dd
