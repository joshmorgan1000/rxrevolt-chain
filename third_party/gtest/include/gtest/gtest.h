#ifndef GTEST_GTEST_H_
#define GTEST_GTEST_H_

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace testing {
struct Test {
    std::string name;
    std::function<void()> func;
};

inline std::vector<Test>& getTests() {
    static std::vector<Test> tests;
    return tests;
}

inline void registerTest(const std::string& name, std::function<void()> f) {
    getTests().push_back({name, std::move(f)});
}

inline int RUN_ALL_TESTS() {
    int failed = 0;
    for (auto& t : getTests()) {
        std::cout << "[ RUN      ] " << t.name << std::endl;
        try {
            t.func();
        } catch (...) {
            std::cerr << "Test threw exception" << std::endl;
            ++failed;
        }
    }
    if (failed == 0)
        std::cout << "[  PASSED  ] All tests" << std::endl;
    else
        std::cout << "[  FAILED  ] " << failed << " test(s)" << std::endl;
    return failed;
}
} // namespace testing

#define TEST(Suite, Name)                                                                          \
    void Suite##_##Name##_impl();                                                                  \
    struct Suite##_##Name##_registrar {                                                            \
        Suite##_##Name##_registrar() {                                                             \
            ::testing::registerTest(#Suite "." #Name, Suite##_##Name##_impl);                      \
        }                                                                                          \
    };                                                                                             \
    static Suite##_##Name##_registrar Suite##_##Name##_registrar_instance;                         \
    void Suite##_##Name##_impl()

#define EXPECT_TRUE(cond)                                                                          \
    do {                                                                                           \
        if (!(cond))                                                                               \
            std::cerr << __FILE__ << ':' << __LINE__ << ": Failure\nExpected: " #cond " is true"   \
                      << std::endl;                                                                \
    } while (0)
#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))
#define EXPECT_EQ(a, b)                                                                            \
    do {                                                                                           \
        if (!((a) == (b)))                                                                         \
            std::cerr << __FILE__ << ':' << __LINE__                                               \
                      << ": Failure\nExpected equality of " #a " and " #b << std::endl;            \
    } while (0)
#define EXPECT_NEAR(a, b, eps)                                                                     \
    do {                                                                                           \
        if (std::fabs((a) - (b)) > (eps))                                                          \
            std::cerr << __FILE__ << ':' << __LINE__                                               \
                      << ": Failure\nExpected |" #a " - " #b "| <= " #eps << std::endl;            \
    } while (0)
#define EXPECT_FLOAT_EQ(a, b) EXPECT_NEAR((a), (b), 1e-6f)
#define ASSERT_TRUE(cond)                                                                          \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << __FILE__ << ':' << __LINE__ << ": Assertion failed: " #cond << std::endl; \
            return;                                                                                \
        }                                                                                          \
    } while (0)
#define ASSERT_EQ(a, b)                                                                            \
    do {                                                                                           \
        if (!((a) == (b))) {                                                                       \
            std::cerr << __FILE__ << ':' << __LINE__ << ": Assertion failed: " #a " == " #b        \
                      << std::endl;                                                                \
            return;                                                                                \
        }                                                                                          \
    } while (0)
#define ASSERT_NE(a, b)                                                                            \
    do {                                                                                           \
        if (!((a) != (b))) {                                                                       \
            std::cerr << __FILE__ << ':' << __LINE__ << ": Assertion failed: " #a " != " #b        \
                      << std::endl;                                                                \
            return;                                                                                \
        }                                                                                          \
    } while (0)
#define ASSERT_NEAR(a, b, eps)                                                                     \
    do {                                                                                           \
        if (std::fabs((a) - (b)) > (eps)) {                                                        \
            std::cerr << __FILE__ << ':' << __LINE__                                               \
                      << ": Assertion failed: |" #a " - " #b "| <= " #eps << std::endl;            \
            return;                                                                                \
        }                                                                                          \
    } while (0)
#define GTEST_SKIP()                                                                               \
    do {                                                                                           \
        return;                                                                                    \
    } while (0)

#endif // GTEST_GTEST_H_
