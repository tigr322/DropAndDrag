#pragma once

// item.hpp — Plain data types for shelf items.
//
// Deliberately free of JSON, database, and platform headers so it can be
// included anywhere in core/, platform/, and ui/ without pulling in heavy
// dependencies.  Serialization lives in the companion item_json.hpp.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dd {

// Discriminator for the kind of content an item holds.
enum class ItemType : uint8_t { File, Folder, Image, Text, URL, Unknown };

// All mutable content fields for an item.
// Optional fields are absent when the data hasn't been fetched or doesn't
// apply to this item type (e.g. url is absent for a File item).
struct ItemData {
    std::string uuid;                               // stable, globally unique id (UUIDv4)
    ItemType    type{ItemType::Unknown};

    std::optional<std::string>          path;           // absolute filesystem path (File/Folder/Image)
    std::optional<std::string>          file_name;      // display name; may differ from path basename
    std::optional<std::string>          text_content;   // full text (Text items)
    std::optional<std::string>          url;            // URL string (URL items)
    std::optional<std::string>          title;          // page title / document title
    std::optional<std::vector<uint8_t>> favicon_data;   // raw PNG/ICO bytes for URL items
    std::optional<std::vector<uint8_t>> thumbnail_data; // cached thumbnail (may be empty; renderer uses QL)
    std::optional<uint64_t>             file_size;      // bytes
    std::optional<std::string>          mime_type;      // "image/png", "application/pdf", …
    std::optional<std::string>          icon_path;      // path to custom icon override

    ItemData() = default;
    ItemData(ItemData&&) = default;
    ItemData& operator=(ItemData&&) = default;
    ItemData(const ItemData&) = default;
    ItemData& operator=(const ItemData&) = default;
};

// Timestamps and organisational tags; kept separate from ItemData so the
// metadata can be updated independently without touching the content fields.
struct ItemMetadata {
    std::chrono::system_clock::time_point created_at {std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point modified_at{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point accessed_at{std::chrono::system_clock::now()};
    bool        is_favorite{false};
    std::vector<std::string>            tags;           // tag names (not ids)
    std::optional<std::string>          collection_id;  // optional parent collection

    ItemMetadata() = default;
    ItemMetadata(ItemMetadata&&) = default;
    ItemMetadata& operator=(ItemMetadata&&) = default;
    ItemMetadata(const ItemMetadata&) = default;
    ItemMetadata& operator=(const ItemMetadata&) = default;
};

// Top-level value type passed through the entire stack: store → renderer → drag.
// Passed by value; the renderer keeps its own copy via shared_ptr<ItemList>.
struct Item {
    ItemData     data;
    ItemMetadata metadata;

    Item() = default;
    Item(Item&&) = default;
    Item& operator=(Item&&) = default;
    Item(const Item&) = default;
    Item& operator=(const Item&) = default;
};

// Helpers for serialising time_points as int64_t milliseconds-since-epoch.
// Used by item_json.hpp and the DB layer; kept here to avoid duplication.
inline int64_t tp_to_ms(const std::chrono::system_clock::time_point& tp) noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()).count();
}

inline std::chrono::system_clock::time_point ms_to_tp(int64_t ms) noexcept {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

} // namespace dd
