#include "event_bus.hpp"

#include <chrono>

namespace dd {

EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

SubscriptionToken EventBus::subscribe(EventType type, EventCallback callback) {
    std::lock_guard lock(mutex_);
    SubscriptionToken token = next_token_.fetch_add(1, std::memory_order_relaxed);
    subscribers_[type].push_back({token, std::move(callback)});
    return token;
}

void EventBus::unsubscribe(SubscriptionToken token) {
    std::lock_guard lock(mutex_);
    for (auto& [type, subs] : subscribers_) {
        std::erase_if(subs, [token](const Subscription& s) {
            return s.token == token;
        });
    }
}

void EventBus::emit(Event event) {
    std::vector<EventCallback> callbacks;
    {
        std::lock_guard lock(mutex_);
        auto it = subscribers_.find(event.type);
        if (it != subscribers_.end()) {
            callbacks.reserve(it->second.size());
            for (const auto& sub : it->second) {
                callbacks.push_back(sub.callback);
            }
        }
    }

    for (const auto& cb : callbacks) {
        cb(event);
    }
}

void EventBus::emit(EventType type, nlohmann::json data) {
    emit(Event{
        .type = type,
        .data = std::move(data),
        .timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count(),
    });
}

} // namespace dd
