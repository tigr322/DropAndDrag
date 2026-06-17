#pragma once

#include <vendor/nlohmann/json.hpp>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
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

struct Event {
    EventType type;
    nlohmann::json data;
    int64_t timestamp;
};

using EventCallback = std::function<void(const Event&)>;
using SubscriptionToken = uint64_t;

class EventBus {
public:
    static EventBus& instance();

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    [[nodiscard]] SubscriptionToken subscribe(EventType type, EventCallback callback);
    void unsubscribe(SubscriptionToken token);
    void emit(Event event);
    void emit(EventType type, nlohmann::json data = {});

private:
    EventBus() = default;

    struct Subscription {
        SubscriptionToken token;
        EventCallback callback;
    };

    mutable std::mutex mutex_;
    std::unordered_map<EventType, std::vector<Subscription>> subscribers_;
    std::atomic<SubscriptionToken> next_token_{1};
};

} // namespace dd
