#include "test_framework.hpp"

#include <core/event_bus/event_bus.hpp>

#include <nlohmann/json.hpp>

#include <atomic>
#include <string>

using namespace dd;

TEST_CASE("EventBus subscribe and emit") {
    auto& bus = EventBus::instance();

    bool received = false;
    auto token = bus.subscribe(EventType::ItemAdded, [&received](const Event& e) {
        received = true;
    });

    bus.emit(EventType::ItemAdded, {{"uuid", "test-1"}});

    ASSERT_TRUE(received);
    bus.unsubscribe(token);
}

TEST_CASE("EventBus multiple subscribers") {
    auto& bus = EventBus::instance();

    std::atomic<int> count{0};

    auto t1 = bus.subscribe(EventType::ItemRemoved, [&count](const Event&) { count.fetch_add(1); });
    auto t2 = bus.subscribe(EventType::ItemRemoved, [&count](const Event&) { count.fetch_add(1); });
    auto t3 = bus.subscribe(EventType::ItemRemoved, [&count](const Event&) { count.fetch_add(1); });

    bus.emit(EventType::ItemRemoved, {{"uuid", "removed-1"}});

    ASSERT_EQ(count.load(), 3);

    bus.unsubscribe(t1);
    bus.unsubscribe(t2);
    bus.unsubscribe(t3);
}

TEST_CASE("EventBus unsubscribe") {
    auto& bus = EventBus::instance();

    int call_count = 0;
    auto token = bus.subscribe(EventType::SettingsChanged, [&call_count](const Event&) {
        call_count++;
    });

    bus.emit(EventType::SettingsChanged);
    ASSERT_EQ(call_count, 1);

    bus.unsubscribe(token);
    bus.emit(EventType::SettingsChanged);
    ASSERT_EQ(call_count, 1);
}

TEST_CASE("EventBus different event types") {
    auto& bus = EventBus::instance();

    bool item_added = false;
    bool item_removed = false;

    auto t1 = bus.subscribe(EventType::ItemAdded, [&](const Event&) { item_added = true; });
    auto t2 = bus.subscribe(EventType::ItemRemoved, [&](const Event&) { item_removed = true; });

    bus.emit(EventType::ItemAdded, {{"uuid", "a"}});

    ASSERT_TRUE(item_added);
    ASSERT_FALSE(item_removed);

    bus.emit(EventType::ItemRemoved, {{"uuid", "b"}});

    ASSERT_TRUE(item_added);
    ASSERT_TRUE(item_removed);

    bus.unsubscribe(t1);
    bus.unsubscribe(t2);
}

TEST_CASE("EventBus event data") {
    auto& bus = EventBus::instance();

    std::string received_uuid;
    std::string received_name;

    auto token = bus.subscribe(EventType::ItemUpdated, [&](const Event& e) {
        if (e.data.contains("uuid")) {
            received_uuid = e.data["uuid"].get<std::string>();
        }
        if (e.data.contains("name")) {
            received_name = e.data["name"].get<std::string>();
        }
    });

    nlohmann::json data;
    data["uuid"] = "item-abc";
    data["name"] = "Test Item";
    data["index"] = 42;

    bus.emit(EventType::ItemUpdated, data);

    ASSERT_EQ(received_uuid, "item-abc");
    ASSERT_EQ(received_name, "Test Item");

    bus.unsubscribe(token);
}

TEST_CASE("EventBus unsubscribe during emission") {
    auto& bus = EventBus::instance();

    std::atomic<int> called{0};
    SubscriptionToken token2 = 0;

    auto t1 = bus.subscribe(EventType::ThemeChanged, [&](const Event&) {
        called.fetch_add(1);
        bus.unsubscribe(token2);
    });

    token2 = bus.subscribe(EventType::ThemeChanged, [&](const Event&) {
        called.fetch_add(1);
    });

    bus.emit(EventType::ThemeChanged, {{"theme", "dark"}});

    ASSERT_GE(called.load(), 1);

    bus.unsubscribe(t1);
}

TEST_CASE("EventBus emit without data") {
    auto& bus = EventBus::instance();

    bool called = false;
    auto token = bus.subscribe(EventType::ShelfHidden, [&](const Event& e) {
        called = true;
        ASSERT_TRUE(e.data.is_object());
        ASSERT_TRUE(e.data.empty());
    });

    bus.emit(EventType::ShelfHidden);

    ASSERT_TRUE(called);
    bus.unsubscribe(token);
}

int main() {
    return dd::test::run_all_tests();
}
