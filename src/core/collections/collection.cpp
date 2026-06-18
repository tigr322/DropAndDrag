// collection.cpp — In-memory CollectionStore (shared_mutex, same pattern as ItemStore).

#include "collection.hpp"

#include <algorithm>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace dd {

class CollectionStore {
public:
    static CollectionStore& instance();

    CollectionStore(const CollectionStore&) = delete;
    CollectionStore& operator=(const CollectionStore&) = delete;
    CollectionStore(CollectionStore&&) = delete;
    CollectionStore& operator=(CollectionStore&&) = delete;

    std::string add(Collection collection);
    std::optional<Collection> get(std::string_view id) const;
    std::vector<Collection> getAll() const;
    bool update(std::string_view id, Collection collection);
    bool remove(std::string_view id);
    [[nodiscard]] size_t count() const noexcept;

private:
    CollectionStore() = default;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Collection> collections_;
};

CollectionStore& CollectionStore::instance() {
    static CollectionStore store;
    return store;
}

std::string CollectionStore::add(Collection collection) {
    std::string id = collection.id;
    std::unique_lock lock(mutex_);
    collections_[id] = std::move(collection);
    return id;
}

std::optional<Collection> CollectionStore::get(std::string_view id) const {
    std::shared_lock lock(mutex_);
    auto it = collections_.find(std::string(id));
    if (it != collections_.end()) return it->second;
    return std::nullopt;
}

std::vector<Collection> CollectionStore::getAll() const {
    std::shared_lock lock(mutex_);
    std::vector<Collection> result;
    result.reserve(collections_.size());
    for (const auto& [key, col] : collections_) {
        result.push_back(col);
    }
    std::sort(result.begin(), result.end(), [](const Collection& a, const Collection& b) {
        return a.order_index < b.order_index;
    });
    return result;
}

bool CollectionStore::update(std::string_view id, Collection collection) {
    std::unique_lock lock(mutex_);
    auto it = collections_.find(std::string(id));
    if (it == collections_.end()) return false;
    it->second = std::move(collection);
    return true;
}

bool CollectionStore::remove(std::string_view id) {
    std::unique_lock lock(mutex_);
    return collections_.erase(std::string(id)) > 0;
}

size_t CollectionStore::count() const noexcept {
    std::shared_lock lock(mutex_);
    return collections_.size();
}

} // namespace dd
