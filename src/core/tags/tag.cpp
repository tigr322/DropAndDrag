// tag.cpp — In-memory TagStore (shared_mutex, same pattern as ItemStore).

#include "tag.hpp"

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dd {

class TagStore {
public:
    static TagStore& instance();

    TagStore(const TagStore&) = delete;
    TagStore& operator=(const TagStore&) = delete;
    TagStore(TagStore&&) = delete;
    TagStore& operator=(TagStore&&) = delete;

    bool add(Tag tag);
    bool remove(std::string_view name);
    std::optional<Tag> get(std::string_view name) const;
    std::vector<Tag> getAll() const;

    std::vector<Tag> getForItem(std::string_view item_uuid) const;
    bool setForItem(std::string_view item_uuid, std::vector<std::string> tag_names);

private:
    TagStore() = default;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Tag> tags_;
    std::unordered_map<std::string, std::unordered_set<std::string>> item_tags_;
};

TagStore& TagStore::instance() {
    static TagStore store;
    return store;
}

bool TagStore::add(Tag tag) {
    std::unique_lock lock(mutex_);
    auto [it, inserted] = tags_.try_emplace(tag.name, std::move(tag));
    return inserted;
}

bool TagStore::remove(std::string_view name) {
    std::unique_lock lock(mutex_);
    if (tags_.erase(std::string(name)) == 0) return false;

    for (auto& [uuid, tag_set] : item_tags_) {
        tag_set.erase(std::string(name));
    }
    return true;
}

std::optional<Tag> TagStore::get(std::string_view name) const {
    std::shared_lock lock(mutex_);
    auto it = tags_.find(std::string(name));
    if (it != tags_.end()) return it->second;
    return std::nullopt;
}

std::vector<Tag> TagStore::getAll() const {
    std::shared_lock lock(mutex_);
    std::vector<Tag> result;
    result.reserve(tags_.size());
    for (const auto& [name, tag] : tags_) {
        result.push_back(tag);
    }
    return result;
}

std::vector<Tag> TagStore::getForItem(std::string_view item_uuid) const {
    std::shared_lock lock(mutex_);
    std::vector<Tag> result;
    auto it = item_tags_.find(std::string(item_uuid));
    if (it != item_tags_.end()) {
        result.reserve(it->second.size());
        for (const auto& tag_name : it->second) {
            auto tag_it = tags_.find(tag_name);
            if (tag_it != tags_.end()) {
                result.push_back(tag_it->second);
            }
        }
    }
    return result;
}

bool TagStore::setForItem(std::string_view item_uuid, std::vector<std::string> tag_names) {
    std::unique_lock lock(mutex_);
    std::string uuid(item_uuid);
    auto& tag_set = item_tags_[uuid];
    tag_set.clear();
    for (auto& name : tag_names) {
        if (tags_.contains(name)) {
            tag_set.insert(std::move(name));
        }
    }
    return true;
}

} // namespace dd
