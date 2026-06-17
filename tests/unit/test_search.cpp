#include "test_framework.hpp"

#include <core/items/item.hpp>

#include <string>
#include <vector>

using namespace dd;

namespace {

struct TestEntry {
    std::string uuid;
    std::string text;
    ItemType type;
};

class SimpleSearchEngine {
public:
    void add(const TestEntry& entry) {
        entries_.push_back(entry);
    }

    void clear() {
        entries_.clear();
    }

    std::vector<TestEntry> search(std::string_view query) const {
        if (query.empty()) {
            return entries_;
        }

        std::vector<TestEntry> results;
        std::string query_lower(query);
        for (auto& c : query_lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        for (const auto& entry : entries_) {
            std::string text_lower(entry.text);
            for (auto& c : text_lower) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (text_lower.find(query_lower) != std::string::npos) {
                results.push_back(entry);
            }
        }

        return results;
    }

    std::vector<TestEntry> search_by_type(ItemType type) const {
        std::vector<TestEntry> results;
        for (const auto& entry : entries_) {
            if (entry.type == type) {
                results.push_back(entry);
            }
        }
        return results;
    }

    std::vector<TestEntry> prefix_search(std::string_view prefix) const {
        if (prefix.empty()) {
            return entries_;
        }

        std::vector<TestEntry> results;
        std::string prefix_lower(prefix);
        for (auto& c : prefix_lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        for (const auto& entry : entries_) {
            std::string text_lower(entry.text);
            for (auto& c : text_lower) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (text_lower.find(prefix_lower) == 0 || text_lower.find(" " + prefix_lower) != std::string::npos) {
                results.push_back(entry);
            }
        }

        return results;
    }

private:
    std::vector<TestEntry> entries_;
};

} // anonymous namespace

TEST_CASE("SearchEngine indexing") {
    SimpleSearchEngine engine;

    engine.add(TestEntry{"1", "Annual Report 2024", ItemType::File});
    engine.add(TestEntry{"2", "Vacation Photos", ItemType::Image});
    engine.add(TestEntry{"3", "Meeting Notes", ItemType::Text});
    engine.add(TestEntry{"4", "https://github.com", ItemType::URL});

    auto results = engine.search("photos");
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].uuid, "2");
}

TEST_CASE("SearchEngine basic text search") {
    SimpleSearchEngine engine;

    engine.add(TestEntry{"a", "Hello World", ItemType::Text});
    engine.add(TestEntry{"b", "Foo Bar", ItemType::Text});
    engine.add(TestEntry{"c", "Baz Qux", ItemType::Text});

    auto results = engine.search("Bar");
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].uuid, "b");
}

TEST_CASE("SearchEngine case insensitive search") {
    SimpleSearchEngine engine;

    engine.add(TestEntry{"x", "UPPERCASE", ItemType::Text});
    engine.add(TestEntry{"y", "lowercase", ItemType::Text});
    engine.add(TestEntry{"z", "MixedCase", ItemType::Text});

    auto results_upper = engine.search("UPPERCASE");
    ASSERT_EQ(results_upper.size(), 1u);

    auto results_lower = engine.search("lowercase");
    ASSERT_EQ(results_lower.size(), 1u);

    auto results_mixed = engine.search("mixedcase");
    ASSERT_EQ(results_mixed.size(), 1u);
}

TEST_CASE("SearchEngine prefix search") {
    SimpleSearchEngine engine;

    engine.add(TestEntry{"1", "Documentation", ItemType::File});
    engine.add(TestEntry{"2", "Document", ItemType::File});
    engine.add(TestEntry{"3", "Docker", ItemType::File});
    engine.add(TestEntry{"4", "Desktop", ItemType::Folder});

    auto results = engine.prefix_search("Doc");
    ASSERT_EQ(results.size(), 2u);

    auto results2 = engine.prefix_search("D");
    ASSERT_EQ(results2.size(), 4u);

    auto results3 = engine.prefix_search("xyz");
    ASSERT_EQ(results3.size(), 0u);
}

TEST_CASE("SearchEngine type filtering") {
    SimpleSearchEngine engine;

    engine.add(TestEntry{"f1", "File Alpha", ItemType::File});
    engine.add(TestEntry{"f2", "File Beta", ItemType::File});
    engine.add(TestEntry{"i1", "Image One", ItemType::Image});
    engine.add(TestEntry{"u1", "URL One", ItemType::URL});

    auto file_results = engine.search_by_type(ItemType::File);
    ASSERT_EQ(file_results.size(), 2u);

    auto image_results = engine.search_by_type(ItemType::Image);
    ASSERT_EQ(image_results.size(), 1u);

    auto url_results = engine.search_by_type(ItemType::URL);
    ASSERT_EQ(url_results.size(), 1u);

    auto text_results = engine.search_by_type(ItemType::Text);
    ASSERT_EQ(text_results.size(), 0u);
}

TEST_CASE("SearchEngine empty query returns all") {
    SimpleSearchEngine engine;

    engine.add(TestEntry{"1", "One", ItemType::Text});
    engine.add(TestEntry{"2", "Two", ItemType::Text});
    engine.add(TestEntry{"3", "Three", ItemType::Text});

    auto results = engine.search("");
    ASSERT_EQ(results.size(), 3u);
}

TEST_CASE("SearchEngine no match") {
    SimpleSearchEngine engine;

    engine.add(TestEntry{"1", "Alpha", ItemType::Text});
    engine.add(TestEntry{"2", "Beta", ItemType::Text});

    auto results = engine.search("NonExistent");
    ASSERT_EQ(results.size(), 0u);
}

int main() {
    return dd::test::run_all_tests();
}
