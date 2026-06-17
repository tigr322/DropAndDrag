# Database Schema

## Configuration

- **Engine**: SQLite 3.35+
- **Journal Mode**: WAL (Write-Ahead Logging) for concurrent reads
- **Foreign Keys**: Enabled
- **Synchronous**: NORMAL (safe with WAL)
- **Cache Size**: 64MB
- **Page Size**: 4096 bytes

## Schema Versioning

```sql
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL DEFAULT (datetime('now')),
    description TEXT
);
```

## Core Tables

### items

The main items table storing all shelf items.

```sql
CREATE TABLE IF NOT EXISTS items (
    uuid TEXT PRIMARY KEY NOT NULL,
    type INTEGER NOT NULL DEFAULT 5,
    path TEXT,
    file_name TEXT,
    text_content TEXT,
    url TEXT,
    title TEXT,
    favicon_data BLOB,
    thumbnail_data BLOB,
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
```

| Column           | Type    | Description                                    |
|------------------|---------|------------------------------------------------|
| uuid             | TEXT    | Primary key, UUID v4 string                    |
| type             | INTEGER | ItemType enum: 0=File, 1=Folder, 2=Image, 3=Text, 4=URL, 5=Unknown |
| path            | TEXT    | File system path (nullable)                    |
| file_name        | TEXT    | Display name (nullable)                        |
| text_content     | TEXT    | Text snippet content (nullable)                |
| url              | TEXT    | URL string (nullable)                          |
| title            | TEXT    | Display title (nullable)                       |
| favicon_data     | BLOB    | Website favicon image data (nullable)          |
| thumbnail_data   | BLOB    | Item thumbnail image data (nullable)           |
| file_size        | INTEGER | File size in bytes (nullable)                  |
| mime_type        | TEXT    | MIME type string (nullable)                    |
| icon_path        | TEXT    | Custom icon path (nullable)                    |
| created_at       | INTEGER | Unix timestamp in milliseconds                 |
| modified_at      | INTEGER | Unix timestamp in milliseconds                 |
| accessed_at      | INTEGER | Unix timestamp in milliseconds                 |
| is_favorite      | INTEGER | Boolean: 0 or 1                               |
| collection_id    | TEXT    | Foreign key to collections (nullable)          |

### collections

Named groups for organizing items.

```sql
CREATE TABLE IF NOT EXISTS collections (
    id TEXT PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    color TEXT NOT NULL DEFAULT '#66BB6A',
    icon TEXT NOT NULL DEFAULT 'folder.svg',
    order_index INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL DEFAULT (strftime('%s','now') * 1000)
);
```

| Column      | Type    | Description                         |
|-------------|---------|-------------------------------------|
| id          | TEXT    | Primary key, UUID v4 string         |
| name        | TEXT    | Collection display name             |
| color       | TEXT    | Hex color string for UI             |
| icon        | TEXT    | Collection icon filename             |
| order_index | INTEGER | Sort order in UI                    |
| created_at  | INTEGER | Unix timestamp in milliseconds       |

### tags

User-defined labels that can be attached to items.

```sql
CREATE TABLE IF NOT EXISTS tags (
    name TEXT PRIMARY KEY NOT NULL,
    color TEXT NOT NULL DEFAULT '#42A5F5',
    created_at INTEGER NOT NULL DEFAULT (strftime('%s','now') * 1000)
);
```

| Column     | Type    | Description                     |
|------------|---------|---------------------------------|
| name       | TEXT    | Primary key, tag name           |
| color      | TEXT    | Hex color string for UI badge   |
| created_at | INTEGER | Unix timestamp in milliseconds   |

### item_tags

Junction table for many-to-many relationship between items and tags.

```sql
CREATE TABLE IF NOT EXISTS item_tags (
    item_uuid TEXT NOT NULL,
    tag_name TEXT NOT NULL,
    PRIMARY KEY (item_uuid, tag_name),
    FOREIGN KEY (item_uuid) REFERENCES items(uuid) ON DELETE CASCADE,
    FOREIGN KEY (tag_name) REFERENCES tags(name) ON DELETE CASCADE
);
```

| Column    | Type | Description                    |
|-----------|------|--------------------------------|
| item_uuid | TEXT | Foreign key to items.uuid      |
| tag_name  | TEXT | Foreign key to tags.name       |

## Search via FTS5

### items_fts

Full-text search virtual table for fast item lookup.

```sql
CREATE VIRTUAL TABLE IF NOT EXISTS items_fts USING fts5(
    uuid,
    file_name,
    text_content,
    url,
    title,
    content='items',
    content_rowid='rowid'
);
```

The FTS5 table uses content-sync mode, meaning it references the `items` table directly for row data. Searchable columns: `file_name`, `text_content`, `url`, `title`.

### FTS5 Triggers

Triggers keep the FTS index synchronized with the items table.

```sql
CREATE TRIGGER IF NOT EXISTS items_ai AFTER INSERT ON items BEGIN
    INSERT INTO items_fts(rowid, uuid, file_name, text_content, url, title)
    VALUES (new.rowid, new.uuid, new.file_name, new.text_content, new.url, new.title);
END;

CREATE TRIGGER IF NOT EXISTS items_ad AFTER DELETE ON items BEGIN
    INSERT INTO items_fts(items_fts, rowid, uuid, file_name, text_content, url, title)
    VALUES ('delete', old.rowid, old.uuid, old.file_name, old.text_content, old.url, old.title);
END;

CREATE TRIGGER IF NOT EXISTS items_au AFTER UPDATE ON items BEGIN
    INSERT INTO items_fts(items_fts, rowid, uuid, file_name, text_content, url, title)
    VALUES ('delete', old.rowid, old.uuid, old.file_name, old.text_content, old.url, old.title);
    INSERT INTO items_fts(rowid, uuid, file_name, text_content, url, title)
    VALUES (new.rowid, new.uuid, new.file_name, new.text_content, new.url, new.title);
END;
```

## Settings

### settings

Key-value store for application settings. Persisted as JSON file on disk, this table serves as runtime cache.

```sql
CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY NOT NULL,
    value TEXT NOT NULL,
    updated_at INTEGER NOT NULL DEFAULT (strftime('%s','now') * 1000)
);
```

## Indexes

```sql
CREATE INDEX IF NOT EXISTS idx_items_type ON items(type);
CREATE INDEX IF NOT EXISTS idx_items_favorite ON items(is_favorite) WHERE is_favorite = 1;
CREATE INDEX IF NOT EXISTS idx_items_collection ON items(collection_id);
CREATE INDEX IF NOT EXISTS idx_items_accessed ON items(accessed_at DESC);
CREATE INDEX IF NOT EXISTS idx_items_created ON items(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_item_tags_item ON item_tags(item_uuid);
CREATE INDEX IF NOT EXISTS idx_item_tags_tag ON item_tags(tag_name);
```

## Migration History

| Version | Description                                                |
|---------|------------------------------------------------------------|
| 1       | Initial schema: items, collections, tags, item_tags, FTS5  |
| 2       | Added settings table                                       |
| 3       | Added indexes on items(accessed_at) and items(created_at)  |
| 4       | Added file_size, mime_type, icon_path to items             |

## Common Queries

### Get all items (newest first)
```sql
SELECT * FROM items ORDER BY accessed_at DESC;
```

### Full-text search
```sql
SELECT i.* FROM items i
INNER JOIN items_fts fts ON i.rowid = fts.rowid
WHERE items_fts MATCH 'query*'
ORDER BY i.accessed_at DESC;
```

### Prefix search (starts-with)
```sql
SELECT i.* FROM items i
INNER JOIN items_fts fts ON i.rowid = fts.rowid
WHERE items_fts MATCH '"query"*'
ORDER BY i.accessed_at DESC;
```

### Get items by type
```sql
SELECT * FROM items WHERE type = 0 ORDER BY accessed_at DESC;
```

### Get favorites
```sql
SELECT * FROM items WHERE is_favorite = 1 ORDER BY accessed_at DESC;
```

### Get items in a collection
```sql
SELECT * FROM items WHERE collection_id = ? ORDER BY accessed_at DESC;
```

### Get items with a tag
```sql
SELECT i.* FROM items i
INNER JOIN item_tags it ON i.uuid = it.item_uuid
WHERE it.tag_name = ?
ORDER BY i.accessed_at DESC;
```

### Get recently accessed items (last 24 hours)
```sql
SELECT * FROM items
WHERE accessed_at > (strftime('%s','now') * 1000) - 86400000
ORDER BY accessed_at DESC
LIMIT 20;
```

### Get tag usage counts
```sql
SELECT t.name, t.color, COUNT(it.item_uuid) as item_count
FROM tags t
LEFT JOIN item_tags it ON t.name = it.tag_name
GROUP BY t.name
ORDER BY item_count DESC;
```

## Performance Notes

- **WAL mode** allows concurrent reads while a write is in progress
- **FTS5 prefix queries** use `"term"*` syntax for efficient prefix matching
- **Covering indexes** avoid table lookups for common filtered queries
- **BLOB storage** for thumbnails avoids filesystem overhead for small images
- **Timestamp format**: milliseconds since Unix epoch for granular sorting
