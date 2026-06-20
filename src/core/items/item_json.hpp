#pragma once

// Include this header only in .cpp files or other headers that explicitly need
// nlohmann::json serialization of Item types. item.hpp itself is json-free.

#include "item.hpp"
#include <vendor/nlohmann/json.hpp>

namespace dd {

NLOHMANN_JSON_SERIALIZE_ENUM(ItemType, {
    {ItemType::File,    "file"},
    {ItemType::Folder,  "folder"},
    {ItemType::Image,   "image"},
    {ItemType::Text,    "text"},
    {ItemType::URL,     "url"},
    {ItemType::Unknown, "unknown"},
})

inline void to_json(nlohmann::json& j, const ItemData& d) {
    j = nlohmann::json::object();
    j["uuid"] = d.uuid;
    j["type"] = d.type;
    if (d.path)           j["path"]           = *d.path;
    if (d.file_name)      j["file_name"]      = *d.file_name;
    if (d.text_content)   j["text_content"]   = *d.text_content;
    if (d.url)            j["url"]            = *d.url;
    if (d.title)          j["title"]          = *d.title;
    if (d.favicon_data)   j["favicon_data"]   = *d.favicon_data;
    if (d.thumbnail_data) j["thumbnail_data"] = *d.thumbnail_data;
    if (d.file_size)      j["file_size"]      = *d.file_size;
    if (d.mime_type)      j["mime_type"]      = *d.mime_type;
    if (d.icon_path)      j["icon_path"]      = *d.icon_path;
}

inline void from_json(const nlohmann::json& j, ItemData& d) {
    j.at("uuid").get_to(d.uuid);
    j.at("type").get_to(d.type);
    if (j.contains("path"))           d.path           = j["path"].get<std::string>();
    if (j.contains("file_name"))      d.file_name      = j["file_name"].get<std::string>();
    if (j.contains("text_content"))   d.text_content   = j["text_content"].get<std::string>();
    if (j.contains("url"))            d.url            = j["url"].get<std::string>();
    if (j.contains("title"))          d.title          = j["title"].get<std::string>();
    if (j.contains("favicon_data"))   d.favicon_data   = j["favicon_data"].get<std::vector<uint8_t>>();
    if (j.contains("thumbnail_data")) d.thumbnail_data = j["thumbnail_data"].get<std::vector<uint8_t>>();
    if (j.contains("file_size"))      d.file_size      = j["file_size"].get<uint64_t>();
    if (j.contains("mime_type"))      d.mime_type      = j["mime_type"].get<std::string>();
    if (j.contains("icon_path"))      d.icon_path      = j["icon_path"].get<std::string>();
}

inline void to_json(nlohmann::json& j, const ItemMetadata& m) {
    j = nlohmann::json::object();
    j["created_at"]   = tp_to_ms(m.created_at);
    j["modified_at"]  = tp_to_ms(m.modified_at);
    j["accessed_at"]  = tp_to_ms(m.accessed_at);
    j["is_favorite"]  = m.is_favorite;
    j["tags"]         = m.tags;
    if (m.collection_id) j["collection_id"] = *m.collection_id;
}

inline void from_json(const nlohmann::json& j, ItemMetadata& m) {
    m.created_at  = ms_to_tp(j.at("created_at").get<int64_t>());
    m.modified_at = ms_to_tp(j.at("modified_at").get<int64_t>());
    m.accessed_at = ms_to_tp(j.at("accessed_at").get<int64_t>());
    j.at("is_favorite").get_to(m.is_favorite);
    j.at("tags").get_to(m.tags);
    if (j.contains("collection_id")) m.collection_id = j["collection_id"].get<std::string>();
}

inline void to_json(nlohmann::json& j, const Item& item) {
    j = nlohmann::json::object();
    j["data"]     = item.data;
    j["metadata"] = item.metadata;
}

inline void from_json(const nlohmann::json& j, Item& item) {
    item.data     = j.at("data").get<ItemData>();
    item.metadata = j.at("metadata").get<ItemMetadata>();
}

} // namespace dd
