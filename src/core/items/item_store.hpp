#pragma once

// item_store.hpp — In-memory implementation of IItemStore.
//
// Backed by std::unordered_map<string, Item> protected by a shared_mutex:
//   • Multiple concurrent readers are allowed (shared lock).
//   • Writers (add/update/remove/clear) take an exclusive lock.
//
// Singleton pattern (test-seam variant):
//   Application owns the concrete instance and registers it at startup via
//   set_instance_for_test().  Unit tests can substitute a mock the same way.
//   See iitem_store.hpp for the test-injection pattern.
//
// NOTE: Currently the store is purely in-memory — items are NOT persisted to
// the Database on mutation.  Wiring store observers → Database::insertItem()
// is listed as a pending task in ARCH_AUDIT.md.

#include "iitem_store.hpp"

#include <shared_mutex>
#include <unordered_map>

namespace dd {

class ItemStore : public IItemStore {
public:
    ItemStore() = default;

    ItemStore(const ItemStore&)            = delete;
    ItemStore& operator=(const ItemStore&) = delete;
    ItemStore(ItemStore&&)                 = delete;
    ItemStore& operator=(ItemStore&&)      = delete;

    // --- Singleton / DI seam ---

    // Returns the process-wide singleton.
    // If set_instance_for_test() has been called with a non-null pointer, returns
    // that pointer instead — allowing tests to inject a mock without changing callers.
    [[nodiscard]] static IItemStore& instance();

    // Override the singleton pointer for testing.
    // Pass nullptr to restore the default (the static-local instance).
    static void set_instance_for_test(IItemStore* p) noexcept;

    // --- IItemStore implementation ---

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
    // Notify all registered observers after a mutation.
    // Called while NOT holding mutex_ (to avoid re-entrancy deadlocks).
    void notify(StoreEvent event, std::string_view uuid);

    // Items map — guarded by mutex_ (shared for reads, exclusive for writes).
    mutable std::shared_mutex                      mutex_;
    std::unordered_map<std::string, Item>          items_;

    // Observer registry — separate mutex so observer callbacks can add/remove
    // other observers without deadlocking on mutex_.
    mutable std::shared_mutex                           observer_mutex_;
    std::unordered_map<uint64_t, StoreObserver>         observers_;
    uint64_t                                            next_observer_token_{1};

    // Pointer override set by set_instance_for_test(); null in production.
    static IItemStore* s_override_;
};

} // namespace dd
