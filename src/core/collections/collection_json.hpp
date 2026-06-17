#pragma once

#include "collection.hpp"
#include <vendor/nlohmann/json.hpp>

namespace dd {

inline void to_json(nlohmann::json& j, const Collection& c) {
    j = nlohmann::json{
        {"id",          c.id},
        {"name",        c.name},
        {"color",       c.color},
        {"icon",        c.icon},
        {"created_at",  std::chrono::duration_cast<std::chrono::milliseconds>(
                            c.created_at.time_since_epoch()).count()},
        {"order_index", c.order_index},
    };
}

inline void from_json(const nlohmann::json& j, Collection& c) {
    j.at("id").get_to(c.id);
    j.at("name").get_to(c.name);
    if (j.contains("color"))       j.at("color").get_to(c.color);
    if (j.contains("icon"))        j.at("icon").get_to(c.icon);
    if (j.contains("created_at"))  c.created_at = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(j.at("created_at").get<int64_t>()));
    if (j.contains("order_index")) j.at("order_index").get_to(c.order_index);
}

} // namespace dd
