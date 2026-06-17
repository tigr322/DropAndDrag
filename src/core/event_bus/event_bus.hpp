#pragma once

#include <any>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dd {

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

// Payload is std::any — producers store typed values; subscribers std::any_cast<T>.
// Keeps this header free of nlohmann, Item, and other heavy includes.
struct Event {
    EventType type;
    std::any  data;
    int64_t   timestamp{};
};

using EventCallback     = std::function<void(const Event&)>;
using SubscriptionToken = uint64_t;

class EventBus {
public:
    EventBus() = default;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    // Process-wide singleton used by production code.
    // Unit tests that need an isolated bus: construct EventBus locally and
    // call set_instance_for_test() before the code under test runs.
    [[nodiscard]] static EventBus& instance();
    static void set_instance_for_test(EventBus* p) noexcept;

    [[nodiscard]] SubscriptionToken subscribe(EventType type, EventCallback callback);
    void unsubscribe(SubscriptionToken token);
    void emit(Event event);
    void emit(EventType type, std::any data = {});

private:
    struct Subscription {
        SubscriptionToken token;
        EventCallback     callback;
    };

    mutable std::mutex mutex_;
    std::unordered_map<EventType, std::vector<Subscription>> subscribers_;
    std::atomic<SubscriptionToken> next_token_{1};

    static EventBus* s_override_;
};

} // namespace dd
