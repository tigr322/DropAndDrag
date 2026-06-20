// event_bus.cpp — Pub/sub event bus implementation.
//
// Thread safety: subscribe/unsubscribe/emit are all guarded by mutex_.
// emit() copies the callback list under the lock and releases before calling
// them, so subscribers can themselves call unsubscribe() without deadlock.
//
// Test isolation: set_instance_for_test(&mock) redirects instance() to the
// provided object.  Clear with nullptr after the test.

#include "event_bus.hpp"

#include <chrono>

namespace dd {

EventBus* EventBus::s_override_ = nullptr;

EventBus& EventBus::instance() {
    if (s_override_) return *s_override_;
    static EventBus bus;
    return bus;
}

void EventBus::set_instance_for_test(EventBus* p) noexcept {
    s_override_ = p;
}

SubscriptionToken EventBus::subscribe(EventType type, EventCallback callback) {
    std::lock_guard lock(mutex_);
    SubscriptionToken token = next_token_++;
    subscribers_[type].push_back({token, std::move(callback)});
    token_to_type_[token] = type;
    return token;
}

void EventBus::unsubscribe(SubscriptionToken token) {
    std::lock_guard lock(mutex_);
    auto map_it = token_to_type_.find(token);
    if (map_it == token_to_type_.end()) return;
    EventType type = map_it->second;
    token_to_type_.erase(map_it);
    auto& subs = subscribers_[type];
    subs.erase(std::find_if(subs.begin(), subs.end(),
        [token](const Subscription& s) { return s.token == token; }));
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

void EventBus::emit(EventType type, std::any data) {
    emit(Event{
        .type      = type,
        .data      = std::move(data),
        .timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count(),
    });
}

} // namespace dd
