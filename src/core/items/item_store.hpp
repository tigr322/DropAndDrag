#pragma once

#include "item.hpp"

#include <functional>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace dd {

enum class StoreEvent : uint8_t { Added, Removed, Updated, Cleared };

using StoreObserver = std::function<void(StoreEvent, std::string_view uuid)>;

class ItemStore {
public:
    static ItemStore& instance();

    ItemStore(const ItemStore&) = delete;
    ItemStore& operator=(const ItemStore&) = delete;
    ItemStore(ItemStore&&) = delete;
    ItemStore& operator=(ItemStore&&) = delete;

    std::string add(Item item);
    std::optional<Item> get(std::string_view uuid) const;
    std::vector<Item> getAll() const;
    bool update(std::string_view uuid, Item item);
    bool remove(std::string_view uuid);
    void clear();
    [[nodiscard]] size_t count() const noexcept;

    uint64_t observe(StoreObserver observer);
    void unobserve(uint64_t token);

private:
    ItemStore() = default;

    void notify(StoreEvent event, std::string_view uuid);

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Item> items_;

    mutable std::shared_mutex observer_mutex_;
    std::unordered_map<uint64_t, StoreObserver> observers_;
    uint64_t next_observer_token_{1};
};

} // namespace dd
