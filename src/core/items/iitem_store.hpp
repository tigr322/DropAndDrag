#pragma once

// iitem_store.hpp — Pure-virtual interface for the item store.
//
// All production code that needs to read or mutate the item list programs
// against this interface, not ItemStore directly.  This makes it trivial to
// inject a mock in unit tests without linking the real SQLite-backed store.
//
// Usage in tests:
//   struct MockStore : dd::IItemStore { ... };
//   MockStore mock;
//   ItemStore::set_instance_for_test(&mock);
//   // … run code under test …
//   ItemStore::set_instance_for_test(nullptr);

#include "item.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dd {

// Events emitted by the store to observers.  Observers receive the uuid of
// the affected item so they can fetch the updated value if needed.
enum class StoreEvent : uint8_t { Added, Removed, Updated, Cleared };

// Callback signature for store observers.
// Called synchronously on the thread that performed the mutation.
using StoreObserver = std::function<void(StoreEvent, std::string_view uuid)>;

class IItemStore {
public:
    virtual ~IItemStore() = default;

    // --- Mutations ---

    // Add a new item.  Generates and assigns item.data.uuid if empty.
    // Returns the uuid that was assigned.
    virtual std::string         add(Item item) = 0;

    // Replace an existing item wholesale.  Returns false if uuid not found.
    virtual bool                update(std::string_view uuid, Item item) = 0;

    // Remove a single item by uuid.  Returns false if not found.
    virtual bool                remove(std::string_view uuid) = 0;

    // Remove all items.
    virtual void                clear() = 0;

    // --- Queries ---

    // Returns nullopt if the uuid does not exist in the store.
    [[nodiscard]] virtual std::optional<Item> get(std::string_view uuid) const = 0;

    // Returns a snapshot of all items in insertion order.
    [[nodiscard]] virtual std::vector<Item>   getAll() const = 0;

    // Number of items currently in the store.
    [[nodiscard]] virtual size_t count() const noexcept = 0;

    // --- Observer pattern ---

    // Register a callback that fires on every mutation.
    // Returns an opaque token; pass it to unobserve() to deregister.
    virtual uint64_t observe(StoreObserver observer) = 0;

    // Deregister a previously registered observer.  No-op if token is invalid.
    virtual void     unobserve(uint64_t token) = 0;
};

} // namespace dd
