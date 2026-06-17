#pragma once

#include "tag.hpp"
#include <vendor/nlohmann/json.hpp>

namespace dd {

inline void to_json(nlohmann::json& j, const Tag& t) {
    j = nlohmann::json{{"name", t.name}, {"color", t.color}};
}

inline void from_json(const nlohmann::json& j, Tag& t) {
    j.at("name").get_to(t.name);
    if (j.contains("color")) j.at("color").get_to(t.color);
}

} // namespace dd
