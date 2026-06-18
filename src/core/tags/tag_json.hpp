#pragma once

// tag_json.hpp — nlohmann/json serialization for Tag and TagStore.
// Include only in .cpp files that actually need JSON conversion.


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
