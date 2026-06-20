#include "test_framework.hpp"

#include <core/items/item.hpp>
#include <core/items/item_json.hpp>
#include <core/items/item_store.hpp>

#include <thread>
#include <vector>

using namespace dd;

TEST_CASE("ItemData default construction") {
    ItemData d;
    ASSERT_TRUE(d.uuid.empty());
    ASSERT_EQ(static_cast<uint8_t>(d.type), static_cast<uint8_t>(ItemType::Unknown));
    ASSERT_FALSE(d.path.has_value());
}

TEST_CASE("ItemData move semantics") {
    ItemData d;
    d.uuid = "test-uuid";
    d.type = ItemType::File;
    d.path = "/test/path.txt";

    ItemData moved = std::move(d);
    ASSERT_EQ(moved.uuid, "test-uuid");
    ASSERT_EQ(static_cast<uint8_t>(moved.type), static_cast<uint8_t>(ItemType::File));
    ASSERT_TRUE(moved.path.has_value());
    ASSERT_EQ(*moved.path, "/test/path.txt");
}

TEST_CASE("ItemMetadata default construction") {
    ItemMetadata m;
    ASSERT_FALSE(m.is_favorite);
    ASSERT_TRUE(m.tags.empty());
}

TEST_CASE("ItemMetadata move semantics") {
    ItemMetadata m;
    m.is_favorite = true;
    m.tags = {"tag1", "tag2"};

    ItemMetadata moved = std::move(m);
    ASSERT_TRUE(moved.is_favorite);
    ASSERT_EQ(moved.tags.size(), 2u);
}

TEST_CASE("Item construction and composition") {
    Item item;
    item.data.uuid = "item-1";
    item.data.type = ItemType::File;
    item.data.file_name = "document.pdf";
    item.metadata.is_favorite = true;

    ASSERT_EQ(item.data.uuid, "item-1");
    ASSERT_EQ(static_cast<uint8_t>(item.data.type), static_cast<uint8_t>(ItemType::File));
    ASSERT_EQ(*item.data.file_name, "document.pdf");
    ASSERT_TRUE(item.metadata.is_favorite);
}

TEST_CASE("Item serialization round-trip") {
    Item item;
    item.data.uuid = "serial-test";
    item.data.type = ItemType::URL;
    item.data.url = "https://example.com";
    item.data.title = "Example";
    item.data.file_size = 1024;
    item.metadata.is_favorite = true;
    item.metadata.tags = {"web", "example"};

    nlohmann::json j = item;
    Item restored = j.get<Item>();

    ASSERT_EQ(restored.data.uuid, item.data.uuid);
    ASSERT_EQ(static_cast<uint8_t>(restored.data.type), static_cast<uint8_t>(item.data.type));
    ASSERT_TRUE(restored.data.url.has_value());
    ASSERT_EQ(*restored.data.url, *item.data.url);
    ASSERT_TRUE(restored.data.title.has_value());
    ASSERT_EQ(*restored.data.title, *item.data.title);
    ASSERT_TRUE(restored.data.file_size.has_value());
    ASSERT_EQ(*restored.data.file_size, *item.data.file_size);
    ASSERT_TRUE(restored.metadata.is_favorite);
    ASSERT_EQ(restored.metadata.tags.size(), 2u);
}

TEST_CASE("ItemStore add and get") {
    auto& store = ItemStore::instance();
    store.clear();

    Item item;
    item.data.uuid = "store-test-1";
    item.data.type = ItemType::Text;
    item.data.text_content = "Hello, World!";

    std::string uuid = store.add(std::move(item));
    ASSERT_FALSE(uuid.empty());

    auto retrieved = store.get(uuid);
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->data.uuid, uuid);
    ASSERT_EQ(static_cast<uint8_t>(retrieved->data.type), static_cast<uint8_t>(ItemType::Text));
    ASSERT_TRUE(retrieved->data.text_content.has_value());
    ASSERT_EQ(*retrieved->data.text_content, "Hello, World!");
}

TEST_CASE("ItemStore update") {
    auto& store = ItemStore::instance();
    store.clear();

    Item item;
    item.data.uuid = "update-test";
    item.data.type = ItemType::File;
    store.add(std::move(item));

    Item updated;
    updated.data.uuid = "update-test";
    updated.data.type = ItemType::URL;
    updated.data.url = "https://updated.example.com";

    bool result = store.update("update-test", std::move(updated));
    ASSERT_TRUE(result);

    auto retrieved = store.get("update-test");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(static_cast<uint8_t>(retrieved->data.type), static_cast<uint8_t>(ItemType::URL));
    ASSERT_TRUE(retrieved->data.url.has_value());
    ASSERT_EQ(*retrieved->data.url, "https://updated.example.com");
}

TEST_CASE("ItemStore remove") {
    auto& store = ItemStore::instance();
    store.clear();

    Item item;
    item.data.uuid = "remove-test";
    store.add(item);

    ASSERT_TRUE(store.get("remove-test").has_value());

    bool removed = store.remove("remove-test");
    ASSERT_TRUE(removed);
    ASSERT_FALSE(store.get("remove-test").has_value());
}

TEST_CASE("ItemStore clear and count") {
    auto& store = ItemStore::instance();
    store.clear();

    ASSERT_EQ(store.count(), 0u);

    for (int i = 0; i < 5; ++i) {
        Item item;
        item.data.uuid = "clear-test-" + std::to_string(i);
        store.add(std::move(item));
    }

    ASSERT_EQ(store.count(), 5u);

    store.clear();
    ASSERT_EQ(store.count(), 0u);
}

TEST_CASE("ItemStore getAll") {
    auto& store = ItemStore::instance();
    store.clear();

    for (int i = 0; i < 3; ++i) {
        Item item;
        item.data.uuid = "getall-" + std::to_string(i);
        store.add(std::move(item));
    }

    auto all = store.getAll();
    ASSERT_EQ(all.size(), 3u);
}

TEST_CASE("ItemStore thread safety concurrent adds") {
    auto& store = ItemStore::instance();
    store.clear();

    constexpr int num_threads = 4;
    constexpr int items_per_thread = 25;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&store, t]() {
            for (int i = 0; i < items_per_thread; ++i) {
                Item item;
                item.data.uuid = "thread-" + std::to_string(t) + "-" + std::to_string(i);
                store.add(std::move(item));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    ASSERT_EQ(store.count(), static_cast<size_t>(num_threads * items_per_thread));
}

TEST_CASE("ItemStore thread safety concurrent reads") {
    auto& store = ItemStore::instance();
    store.clear();

    Item item;
    item.data.uuid = "shared-read";
    store.add(std::move(item));

    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&store]() {
            for (int i = 0; i < 100; ++i) {
                auto result = store.get("shared-read");
                ASSERT_TRUE(result.has_value());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

int main() {
    return dd::test::run_all_tests();
}
