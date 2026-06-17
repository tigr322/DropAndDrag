#pragma once

#include "../items/item.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace dd {

class SearchEngine {
public:
    static SearchEngine& instance();

    SearchEngine(const SearchEngine&) = delete;
    SearchEngine& operator=(const SearchEngine&) = delete;
    SearchEngine(SearchEngine&&) = delete;
    SearchEngine& operator=(SearchEngine&&) = delete;

    [[nodiscard]] std::vector<Item> search(std::string_view query);
    [[nodiscard]] std::vector<Item> searchByType(ItemType type);
    [[nodiscard]] std::vector<Item> searchInCollection(std::string_view collection_id,
                                                         std::string_view query);
    void indexItem(const Item& item);
    void deindexItem(std::string_view uuid);

    void set_db(sqlite3* db) noexcept { db_ = db; }

private:
    SearchEngine() = default;

    sqlite3* db_{nullptr};
    std::mutex mutex_;
};

} // namespace dd
