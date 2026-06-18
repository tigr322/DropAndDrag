#pragma once

// event_bus.hpp — Pub/sub event system.
//
// Components emit typed events; other components subscribe to specific types.
// The payload (Event::data) is std::any, keeping this header free of Item,
// Collection, and other heavy types.  Producers store a typed value:
//
//   bus.emit(EventType::ItemAdded, std::string{uuid});
//
// Subscribers cast back:
//
//   bus.subscribe(EventType::ItemAdded, [](const Event& e) {
//       auto uuid = std::any_cast<std::string>(e.data);
//   });
//
// Thread safety: subscribe/unsubscribe/emit are all mutex-protected and safe
// to call from any thread.  Callbacks fire on the thread that called emit().
//
// NOTE: As of 2026-06-18 the bus is constructed but no subscribers or emitters
// are wired.  See ARCH_AUDIT.md §10.

#include <any>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dd {

// All event types in the system.  Stored as uint8_t; do not reorder.
enum class EventType : uint8_t {
    ItemAdded,
    ItemRemoved,
    ItemUpdated,
    CollectionChanged,
    TagChanged,
    SettingsChanged,
    ShelfShown,
    ShelfHidden,
    SearchQueryChanged,
    ThemeChanged,
    DragEnter,
    DragLeave,
    Drop,
    WindowFocusChanged,
};

// A single bus event.  data is std::any — cast it to the type the producer
// documented for each EventType.
struct Event {
    EventType type;
    std::any  data;         // payload; may be empty ({}) for notification-only events
    int64_t   timestamp{};  // milliseconds since epoch; filled by emit()
};

// Callback invoked for every event of the subscribed type.
using EventCallback = std::function<void(const Event&)>;

// Opaque handle returned by subscribe(); used to cancel with unsubscribe().
using SubscriptionToken = uint64_t;

class EventBus {
public:
    EventBus() = default;

    EventBus(const EventBus&)            = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&)                 = delete;
    EventBus& operator=(EventBus&&)      = delete;

    // --- Singleton / DI seam ---
    // Same pattern as ItemStore: Application registers the real instance; tests
    // can substitute an isolated EventBus via set_instance_for_test().

    [[nodiscard]] static EventBus& instance();
    static void set_instance_for_test(EventBus* p) noexcept;

    // --- API ---

    // Register a callback for a specific event type.
    // The returned token is required to cancel the subscription.
    [[nodiscard]] SubscriptionToken subscribe(EventType type, EventCallback callback);

    // Cancel a subscription.  Safe to call with an invalid token (no-op).
    void unsubscribe(SubscriptionToken token);

    // Emit a pre-built Event (timestamp is taken from Event::timestamp if set).
    void emit(Event event);

    // Convenience overload: constructs Event{type, data} and emits it.
    void emit(EventType type, std::any data = {});

private:
    struct Subscription {
        SubscriptionToken token;
        EventCallback     callback;
    };

    mutable std::mutex mutex_;
    // Map from event type to all active subscriptions for that type.
    std::unordered_map<EventType, std::vector<Subscription>> subscribers_;
    // Monotonically increasing token counter.
    std::atomic<SubscriptionToken> next_token_{1};

    // Test-injection override; null in production (uses static-local instance).
    static EventBus* s_override_;
};

} // namespace dd
