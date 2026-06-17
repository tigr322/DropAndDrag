#include "item_store.hpp"

#include <algorithm>
#include <shared_mutex>

namespace dd {

ItemStore& ItemStore::instance() {
    static ItemStore store;
    return store;
}

std::string ItemStore::add(Item item) {
    std::string uuid = item.data.uuid;
    {
        std::unique_lock lock(mutex_);
        items_[uuid] = std::move(item);
    }
    notify(StoreEvent::Added, uuid);
    return uuid;
}

std::optional<Item> ItemStore::get(std::string_view uuid) const {
    std::shared_lock lock(mutex_);
    auto it = items_.find(std::string(uuid));
    if (it != items_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<Item> ItemStore::getAll() const {
    std::shared_lock lock(mutex_);
    std::vector<Item> result;
    result.reserve(items_.size());
    for (const auto& [key, item] : items_) {
        result.push_back(item);
    }
    return result;
}

bool ItemStore::update(std::string_view uuid, Item item) {
    {
        std::unique_lock lock(mutex_);
        auto it = items_.find(std::string(uuid));
        if (it == items_.end()) return false;
        it->second = std::move(item);
    }
    notify(StoreEvent::Updated, uuid);
    return true;
}

bool ItemStore::remove(std::string_view uuid) {
    {
        std::unique_lock lock(mutex_);
        if (items_.erase(std::string(uuid)) == 0) return false;
    }
    notify(StoreEvent::Removed, uuid);
    return true;
}

void ItemStore::clear() {
    {
        std::unique_lock lock(mutex_);
        items_.clear();
    }
    notify(StoreEvent::Cleared, {});
}

size_t ItemStore::count() const noexcept {
    std::shared_lock lock(mutex_);
    return items_.size();
}

uint64_t ItemStore::observe(StoreObserver observer) {
    std::unique_lock lock(observer_mutex_);
    uint64_t token = next_observer_token_++;
    observers_[token] = std::move(observer);
    return token;
}

void ItemStore::unobserve(uint64_t token) {
    std::unique_lock lock(observer_mutex_);
    observers_.erase(token);
}

void ItemStore::notify(StoreEvent event, std::string_view uuid) {
    std::shared_lock lock(observer_mutex_);
    for (const auto& [token, observer] : observers_) {
        observer(event, uuid);
    }
}

} // namespace dd
