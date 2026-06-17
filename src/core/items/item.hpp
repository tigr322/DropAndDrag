#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dd {

enum class ItemType : uint8_t { File, Folder, Image, Text, URL, Unknown };

struct ItemData {
    std::string uuid;
    ItemType type{ItemType::Unknown};
    std::optional<std::string> path;
    std::optional<std::string> file_name;
    std::optional<std::string> text_content;
    std::optional<std::string> url;
    std::optional<std::string> title;
    std::optional<std::vector<uint8_t>> favicon_data;
    std::optional<std::vector<uint8_t>> thumbnail_data;
    std::optional<uint64_t> file_size;
    std::optional<std::string> mime_type;
    std::optional<std::string> icon_path;

    ItemData() = default;
    ItemData(ItemData&&) = default;
    ItemData& operator=(ItemData&&) = default;
    ItemData(const ItemData&) = default;
    ItemData& operator=(const ItemData&) = default;
};

struct ItemMetadata {
    std::string uuid;
    std::chrono::system_clock::time_point created_at{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point modified_at{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point accessed_at{std::chrono::system_clock::now()};
    bool is_favorite{false};
    std::vector<std::string> tags;
    std::optional<std::string> collection_id;

    ItemMetadata() = default;
    ItemMetadata(ItemMetadata&&) = default;
    ItemMetadata& operator=(ItemMetadata&&) = default;
    ItemMetadata(const ItemMetadata&) = default;
    ItemMetadata& operator=(const ItemMetadata&) = default;
};

struct Item {
    ItemData data;
    ItemMetadata metadata;

    Item() = default;
    Item(Item&&) = default;
    Item& operator=(Item&&) = default;
    Item(const Item&) = default;
    Item& operator=(const Item&) = default;
};

inline int64_t tp_to_ms(const std::chrono::system_clock::time_point& tp) noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

inline std::chrono::system_clock::time_point ms_to_tp(int64_t ms) noexcept {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

} // namespace dd
