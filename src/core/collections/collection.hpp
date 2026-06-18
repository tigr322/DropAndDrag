#pragma once

// collection.hpp — Collection struct and in-memory CollectionStore.
// JSON-free; use collection_json.hpp for serialization.


#include <chrono>
#include <string>

namespace dd {

struct Collection {
    std::string id;
    std::string name;
    std::string color{"#3B82F6"};
    std::string icon{"folder"};
    std::chrono::system_clock::time_point created_at{std::chrono::system_clock::now()};
    int order_index{0};
};

} // namespace dd
