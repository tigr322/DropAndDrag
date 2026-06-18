// migrations.cpp — Versioned SQLite schema upgrades.
//
// Each migration runs in its own BEGIN IMMEDIATE / COMMIT transaction via
// run_in_transaction().  On failure, ROLLBACK leaves the DB at the previous
// version; the next application start sees the same version number and retries.
// user_version (SQLite PRAGMA) is bumped to the target version only after a
// successful COMMIT, so the version number is always the highest fully-applied
// migration.
//
// Adding a migration:
//   1. Implement bool migrate_vN(sqlite3*) — all DDL in one go.
//   2. Add  if (version < N) { run_in_transaction(db, N, migrate_vN); ... }
//      to run_migrations(), following the existing pattern.
//   3. Bump kCurrentVersion.

#include "migrations.hpp"

#include <sqlite3.h>

#include <string>
#include <string_view>

namespace dd {

namespace {

constexpr int kCurrentVersion = 1;

// Returns true on success. Silently frees any SQLite error message.
bool exec(sqlite3* db, std::string_view sql) {
    char* err = nullptr;
    int   rc  = sqlite3_exec(db, sql.data(), nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    return rc == SQLITE_OK;
}

int get_user_version(sqlite3* db) {
    sqlite3_stmt* s = nullptr;
    sqlite3_prepare_v3(db, "PRAGMA user_version;", -1, 0, &s, nullptr);
    int version = 0;
    if (s && sqlite3_step(s) == SQLITE_ROW) {
        version = sqlite3_column_int(s, 0);
    }
    if (s) sqlite3_finalize(s);
    return version;
}

void set_user_version(sqlite3* db, int version) {
    exec(db, "PRAGMA user_version = " + std::to_string(version) + ";");
}

// Returns true if every DDL statement succeeded.
bool migrate_v1(sqlite3* db) {
    bool ok = true;

    ok = ok && exec(db, R"(
        CREATE TABLE IF NOT EXISTS items (
            uuid         TEXT    PRIMARY KEY,
            type         INTEGER NOT NULL DEFAULT 5,
            path         TEXT,
            file_name    TEXT,
            text_content TEXT,
            url          TEXT,
            title        TEXT,
            file_size    INTEGER,
            mime_type    TEXT,
            icon_path    TEXT,
            created_at   INTEGER NOT NULL,
            modified_at  INTEGER NOT NULL,
            accessed_at  INTEGER NOT NULL,
            is_favorite  INTEGER NOT NULL DEFAULT 0,
            collection_id TEXT,
            FOREIGN KEY (collection_id) REFERENCES collections(id) ON DELETE SET NULL
        );
    )");

    ok = ok && exec(db, R"(
        CREATE TABLE IF NOT EXISTS collections (
            id          TEXT    PRIMARY KEY,
            name        TEXT    NOT NULL,
            color       TEXT    NOT NULL DEFAULT '#3B82F6',
            icon        TEXT    NOT NULL DEFAULT 'folder',
            order_index INTEGER NOT NULL DEFAULT 0,
            created_at  INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
        );
    )");

    ok = ok && exec(db, R"(
        CREATE TABLE IF NOT EXISTS tags (
            name  TEXT PRIMARY KEY,
            color TEXT NOT NULL DEFAULT '#6B7280'
        );
    )");

    ok = ok && exec(db, R"(
        CREATE TABLE IF NOT EXISTS item_tags (
            item_uuid TEXT NOT NULL,
            tag_name  TEXT NOT NULL,
            PRIMARY KEY (item_uuid, tag_name),
            FOREIGN KEY (item_uuid) REFERENCES items(uuid) ON DELETE CASCADE,
            FOREIGN KEY (tag_name)  REFERENCES tags(name)  ON DELETE CASCADE
        );
    )");

    ok = ok && exec(db, R"(
        CREATE TABLE IF NOT EXISTS settings (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )");

    ok = ok && exec(db, R"(
        CREATE VIRTUAL TABLE IF NOT EXISTS search_index USING fts5(
            uuid         UNINDEXED,
            title,
            file_name,
            text_content,
            url,
            content='items',
            content_rowid='rowid'
        );
    )");

    ok = ok && exec(db, R"(
        CREATE TRIGGER IF NOT EXISTS items_ai AFTER INSERT ON items BEGIN
            INSERT INTO search_index(rowid, title, file_name, text_content, url)
            VALUES (new.rowid, new.title, new.file_name, new.text_content, new.url);
        END;
    )");

    ok = ok && exec(db, R"(
        CREATE TRIGGER IF NOT EXISTS items_ad AFTER DELETE ON items BEGIN
            INSERT INTO search_index(search_index, rowid, title, file_name, text_content, url)
            VALUES ('delete', old.rowid, old.title, old.file_name, old.text_content, old.url);
        END;
    )");

    ok = ok && exec(db, R"(
        CREATE TRIGGER IF NOT EXISTS items_au AFTER UPDATE ON items BEGIN
            INSERT INTO search_index(search_index, rowid, title, file_name, text_content, url)
            VALUES ('delete', old.rowid, old.title, old.file_name, old.text_content, old.url);
            INSERT INTO search_index(rowid, title, file_name, text_content, url)
            VALUES (new.rowid, new.title, new.file_name, new.text_content, new.url);
        END;
    )");

    ok = ok && exec(db, "CREATE INDEX IF NOT EXISTS idx_items_type       ON items(type);");
    ok = ok && exec(db, "CREATE INDEX IF NOT EXISTS idx_items_collection  ON items(collection_id);");
    ok = ok && exec(db, "CREATE INDEX IF NOT EXISTS idx_items_favorite    ON items(is_favorite);");
    ok = ok && exec(db, "CREATE INDEX IF NOT EXISTS idx_items_modified    ON items(modified_at DESC);");
    ok = ok && exec(db, "CREATE INDEX IF NOT EXISTS idx_item_tags_item    ON item_tags(item_uuid);");
    ok = ok && exec(db, "CREATE INDEX IF NOT EXISTS idx_item_tags_tag     ON item_tags(tag_name);");

    return ok;
}

// Helper: run one migration inside a BEGIN IMMEDIATE / COMMIT/ROLLBACK block.
// Returns true if committed successfully.
bool run_in_transaction(sqlite3* db, int target_version, bool (*migration)(sqlite3*)) {
    if (!exec(db, "BEGIN IMMEDIATE;")) return false;
    if (!migration(db)) {
        exec(db, "ROLLBACK;");
        return false;
    }
    set_user_version(db, target_version);
    if (!exec(db, "COMMIT;")) {
        exec(db, "ROLLBACK;");
        return false;
    }
    return true;
}

} // anonymous namespace

void run_migrations(sqlite3* db) {
    const int version = get_user_version(db);

    if (version < 1) {
        run_in_transaction(db, 1, migrate_v1);
        // If the transaction failed the DB is unchanged; subsequent opens will
        // retry from version 0.  Do not attempt further migrations this run.
        if (get_user_version(db) < 1) return;
    }

    // Future migrations follow the same pattern:
    // if (version < 2) {
    //     run_in_transaction(db, 2, migrate_v2);
    //     if (get_user_version(db) < 2) return;
    // }

    (void)kCurrentVersion; // suppress unused-variable warning when all migrations are applied
}

} // namespace dd
