#include "test_framework.hpp"

#include <core/items/item.hpp>
#include <core/database/db.hpp>

#include <filesystem>
#include <chrono>
#include <thread>
#include <vector>
#include <cstdio>

using namespace dd;

namespace {

std::string temp_db_path() {
    auto path = std::filesystem::temp_directory_path() / "dropanddrag_test.db";
    return path.string();
}

void remove_db(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path + "-wal", ec);
    std::filesystem::remove(path + "-shm", ec);
}

} // anonymous namespace

static std::string g_db_path;

TEST_CASE("Database initialization") {
    g_db_path = temp_db_path();
    remove_db(g_db_path);

    auto& db = Database::instance();
    auto result = db.init(g_db_path);
    result.wait();

    ASSERT_TRUE(result.get());
    ASSERT_TRUE(db.is_open());
}

TEST_CASE("Database insert and get item") {
    auto& db = Database::instance();

    Item item;
    item.data.uuid = "db-test-item-1";
    item.data.type = ItemType::File;
    item.data.file_name = "report.pdf";
    item.data.path = "/home/user/report.pdf";
    item.data.file_size = 2048;
    item.metadata.uuid = "db-test-item-1";
    item.metadata.is_favorite = true;
    item.metadata.tags = {"work", "pdf"};

    auto insert_result = db.insertItem(item);
    insert_result.wait();
    ASSERT_TRUE(insert_result.get());

    auto get_result = db.getItem("db-test-item-1");
    get_result.wait();
    auto retrieved = get_result.get();

    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->data.uuid, "db-test-item-1");
    ASSERT_EQ(static_cast<uint8_t>(retrieved->data.type), static_cast<uint8_t>(ItemType::File));
    ASSERT_TRUE(retrieved->data.file_name.has_value());
    ASSERT_EQ(*retrieved->data.file_name, "report.pdf");
    ASSERT_TRUE(retrieved->metadata.is_favorite);
    ASSERT_EQ(retrieved->metadata.tags.size(), 2u);
}

TEST_CASE("Database update item") {
    auto& db = Database::instance();

    Item updated;
    updated.data.uuid = "db-test-item-1";
    updated.data.type = ItemType::Text;
    updated.data.file_name = "updated.txt";
    updated.metadata.uuid = "db-test-item-1";
    updated.metadata.is_favorite = false;

    auto result = db.updateItem(updated);
    result.wait();
    ASSERT_TRUE(result.get());

    auto get_result = db.getItem("db-test-item-1");
    get_result.wait();
    auto retrieved = get_result.get();

    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(static_cast<uint8_t>(retrieved->data.type), static_cast<uint8_t>(ItemType::Text));
    ASSERT_FALSE(retrieved->metadata.is_favorite);
}

TEST_CASE("Database delete item") {
    auto& db = Database::instance();

    auto del_result = db.deleteItem("db-test-item-1");
    del_result.wait();
    ASSERT_TRUE(del_result.get());

    auto get_result = db.getItem("db-test-item-1");
    get_result.wait();
    ASSERT_FALSE(get_result.get().has_value());
}

TEST_CASE("Database insert multiple items") {
    auto& db = Database::instance();

    for (int i = 0; i < 10; ++i) {
        Item item;
        item.data.uuid = "bulk-item-" + std::to_string(i);
        item.data.type = ItemType::File;
        item.data.file_name = "file_" + std::to_string(i) + ".txt";
        item.metadata.uuid = "bulk-item-" + std::to_string(i);

        auto result = db.insertItem(item);
        result.wait();
        ASSERT_TRUE(result.get());
    }

    auto all_result = db.getAllItems();
    all_result.wait();
    auto all = all_result.get();

    ASSERT_TRUE(all.size() >= 10);
}

TEST_CASE("Database collection operations") {
    auto& db = Database::instance();

    auto insert = db.insertCollection("col-1", "Work", "#FF5733", "folder.svg", 0);
    insert.wait();
    ASSERT_TRUE(insert.get());

    auto get = db.getCollections();
    get.wait();
    auto collections = get.get();
    ASSERT_TRUE(collections.size() > 0);

    auto upd = db.updateCollection("col-1", "Personal", "#33FF57", "home.svg", 1);
    upd.wait();
    ASSERT_TRUE(upd.get());

    auto del = db.deleteCollection("col-1");
    del.wait();
    ASSERT_TRUE(del.get());
}

TEST_CASE("Database tag operations") {
    auto& db = Database::instance();

    auto add1 = db.addTag("important", "#FF0000");
    add1.wait();
    ASSERT_TRUE(add1.get());

    auto add2 = db.addTag("archive", "#0000FF");
    add2.wait();
    ASSERT_TRUE(add2.get());

    auto get = db.getTags();
    get.wait();
    auto tags = get.get();
    ASSERT_TRUE(tags.size() >= 2);

    auto rem = db.removeTag("archive");
    rem.wait();
    ASSERT_TRUE(rem.get());
}

TEST_CASE("Database favorites") {
    auto& db = Database::instance();

    Item fav_item;
    fav_item.data.uuid = "fav-item";
    fav_item.data.type = ItemType::URL;
    fav_item.data.url = "https://favorite.example.com";
    fav_item.metadata.uuid = "fav-item";

    auto ins = db.insertItem(fav_item);
    ins.wait();

    auto fav = db.setFavorite("fav-item", true);
    fav.wait();
    ASSERT_TRUE(fav.get());

    auto get_favs = db.getFavorites();
    get_favs.wait();
    auto favs = get_favs.get();
    ASSERT_TRUE(favs.size() > 0);

    db.deleteItem("fav-item").wait();
}

TEST_CASE("Database concurrent access") {
    auto& db = Database::instance();

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&db, t]() {
            for (int i = 0; i < 5; ++i) {
                Item item;
                item.data.uuid = "concurrent-" + std::to_string(t) + "-" + std::to_string(i);
                item.data.type = ItemType::Text;
                item.metadata.uuid = "concurrent-" + std::to_string(t) + "-" + std::to_string(i);

                auto result = db.insertItem(item);
                result.wait();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto all = db.getAllItems();
    all.wait();
    ASSERT_TRUE(all.get().size() > 0);
}

TEST_CASE("Database close") {
    auto& db = Database::instance();

    auto result = db.close();
    result.wait();

    ASSERT_FALSE(db.is_open());

    remove_db(g_db_path);
}

int main() {
    return dd::test::run_all_tests();
}
