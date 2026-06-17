#pragma once

#include <vendor/nlohmann/json.hpp>

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

namespace json_util {

NLOHMANN_JSON_SERIALIZE_ENUM(ItemType, {
    {ItemType::File, "file"},
    {ItemType::Folder, "folder"},
    {ItemType::Image, "image"},
    {ItemType::Text, "text"},
    {ItemType::URL, "url"},
    {ItemType::Unknown, "unknown"},
})

inline void to_json(nlohmann::json& j, const std::chrono::system_clock::time_point& tp) {
    j = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

inline void from_json(const nlohmann::json& j, std::chrono::system_clock::time_point& tp) {
    tp = std::chrono::system_clock::time_point(std::chrono::milliseconds(j.get<int64_t>()));
}

inline void to_json(nlohmann::json& j, const ItemData& d) {
    j = nlohmann::json{
        {"uuid", d.uuid},
        {"type", d.type},
    };
    if (d.path) j["path"] = *d.path;
    if (d.file_name) j["file_name"] = *d.file_name;
    if (d.text_content) j["text_content"] = *d.text_content;
    if (d.url) j["url"] = *d.url;
    if (d.title) j["title"] = *d.title;
    if (d.favicon_data) j["favicon_data"] = *d.favicon_data;
    if (d.thumbnail_data) j["thumbnail_data"] = *d.thumbnail_data;
    if (d.file_size) j["file_size"] = *d.file_size;
    if (d.mime_type) j["mime_type"] = *d.mime_type;
    if (d.icon_path) j["icon_path"] = *d.icon_path;
}

inline void from_json(const nlohmann::json& j, ItemData& d) {
    j.at("uuid").get_to(d.uuid);
    j.at("type").get_to(d.type);
    if (j.contains("path")) d.path = j["path"].get<std::string>();
    if (j.contains("file_name")) d.file_name = j["file_name"].get<std::string>();
    if (j.contains("text_content")) d.text_content = j["text_content"].get<std::string>();
    if (j.contains("url")) d.url = j["url"].get<std::string>();
    if (j.contains("title")) d.title = j["title"].get<std::string>();
    if (j.contains("favicon_data")) d.favicon_data = j["favicon_data"].get<std::vector<uint8_t>>();
    if (j.contains("thumbnail_data")) d.thumbnail_data = j["thumbnail_data"].get<std::vector<uint8_t>>();
    if (j.contains("file_size")) d.file_size = j["file_size"].get<uint64_t>();
    if (j.contains("mime_type")) d.mime_type = j["mime_type"].get<std::string>();
    if (j.contains("icon_path")) d.icon_path = j["icon_path"].get<std::string>();
}

inline void to_json(nlohmann::json& j, const ItemMetadata& m) {
    j = nlohmann::json{
        {"uuid", m.uuid},
        {"created_at", m.created_at},
        {"modified_at", m.modified_at},
        {"accessed_at", m.accessed_at},
        {"is_favorite", m.is_favorite},
        {"tags", m.tags},
    };
    if (m.collection_id) j["collection_id"] = *m.collection_id;
}

inline void from_json(const nlohmann::json& j, ItemMetadata& m) {
    j.at("uuid").get_to(m.uuid);
    j.at("created_at").get_to(m.created_at);
    j.at("modified_at").get_to(m.modified_at);
    j.at("accessed_at").get_to(m.accessed_at);
    j.at("is_favorite").get_to(m.is_favorite);
    j.at("tags").get_to(m.tags);
    if (j.contains("collection_id")) m.collection_id = j["collection_id"].get<std::string>();
}

inline void to_json(nlohmann::json& j, const Item& item) {
    j = nlohmann::json{
        {"data", item.data},
        {"metadata", item.metadata},
    };
}

inline void from_json(const nlohmann::json& j, Item& item) {
    j.at("data").get_to(item.data);
    j.at("metadata").get_to(item.metadata);
}

} // namespace json_util

} // namespace dd
