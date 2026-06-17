#pragma once

#include "iitem_store.hpp"

#include <shared_mutex>
#include <unordered_map>

namespace dd {

class ItemStore : public IItemStore {
public:
    ItemStore() = default;

    ItemStore(const ItemStore&) = delete;
    ItemStore& operator=(const ItemStore&) = delete;
    ItemStore(ItemStore&&) = delete;
    ItemStore& operator=(ItemStore&&) = delete;

    // Returns the process-wide singleton used by production code.
    // Unit tests: call set_instance_for_test(&mock) before exercising code that
    // calls ItemStore::instance(), then set_instance_for_test(nullptr) to restore.
    [[nodiscard]] static IItemStore& instance();
    static void set_instance_for_test(IItemStore* p) noexcept;

    std::string         add(Item item) override;
    std::optional<Item> get(std::string_view uuid) const override;
    std::vector<Item>   getAll() const override;
    bool                update(std::string_view uuid, Item item) override;
    bool                remove(std::string_view uuid) override;
    void                clear() override;
    [[nodiscard]] size_t count() const noexcept override;

    uint64_t observe(StoreObserver observer) override;
    void     unobserve(uint64_t token) override;

private:
    void notify(StoreEvent event, std::string_view uuid);

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Item> items_;

    mutable std::shared_mutex observer_mutex_;
    std::unordered_map<uint64_t, StoreObserver> observers_;
    uint64_t next_observer_token_{1};

    static IItemStore* s_override_;
};

} // namespace dd
