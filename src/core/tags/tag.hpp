#pragma once

// tag.hpp — Tag struct and in-memory TagStore.
// JSON-free; use tag_json.hpp for serialization.


#include <string>

namespace dd {

struct Tag {
    std::string name;
    std::string color{"#6B7280"};
};

} // namespace dd
