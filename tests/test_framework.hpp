#pragma once

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace dd::test {

struct TestCase {
    std::string name;
    void (*func)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> reg;
    return reg;
}

struct TestRegistrar {
    TestRegistrar(std::string_view name, void (*func)()) {
        registry().push_back(TestCase{std::string(name), func});
    }
};

#define DD_TEST_CONCAT_IMPL(x, y) x##y
#define DD_TEST_CONCAT(x, y) DD_TEST_CONCAT_IMPL(x, y)

#define TEST_CASE(name)                                                        \
    static void DD_TEST_CONCAT(test_func_, __LINE__)();                        \
    static ::dd::test::TestRegistrar DD_TEST_CONCAT(reg_, __LINE__)(            \
        name, DD_TEST_CONCAT(test_func_, __LINE__));                           \
    static void DD_TEST_CONCAT(test_func_, __LINE__)()

inline void check(bool condition, std::string_view message,
                  std::source_location loc = std::source_location::current()) {
    if (!condition) {
        std::cerr << "  FAIL: " << loc.file_name() << ":" << loc.line()
                  << " - " << message << "\n";
        std::exit(1);
    }
}

#define ASSERT_TRUE(cond)  ::dd::test::check((cond), #cond)
#define ASSERT_FALSE(cond) ::dd::test::check(!(cond), #cond)
#define ASSERT_EQ(a, b)    ::dd::test::check((a) == (b), #a " == " #b)
#define ASSERT_NE(a, b)    ::dd::test::check((a) != (b), #a " != " #b)
#define ASSERT_GE(a, b)    ::dd::test::check((a) >= (b), #a " >= " #b)
#define ASSERT_GT(a, b)    ::dd::test::check((a) >  (b), #a " > "  #b)
#define ASSERT_LE(a, b)    ::dd::test::check((a) <= (b), #a " <= " #b)
#define ASSERT_LT(a, b)    ::dd::test::check((a) <  (b), #a " < "  #b)

inline int run_all_tests() {
    const auto& tests = registry();
    size_t passed = 0;

    std::cout << "Running " << tests.size() << " test(s)\n\n";

    for (const auto& test : tests) {
        std::cout << "[ RUN      ] " << test.name << "\n";
        try {
            test.func();
            std::cout << "[       OK ] " << test.name << "\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cerr << "[     FAIL ] " << test.name << " - " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[     FAIL ] " << test.name << " - unknown exception\n";
        }
    }

    std::cout << "\n" << passed << " / " << tests.size() << " test(s) passed\n";

    return (passed == tests.size()) ? 0 : 1;
}

} // namespace dd::test
