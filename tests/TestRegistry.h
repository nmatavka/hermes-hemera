#pragma once

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace hermes::tests {

using TestFn = std::function<void()>;

struct TestCase {
    std::string name;
    TestFn fn;
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> registry;
    return registry;
}

struct Registrar {
    Registrar(std::string test_name, TestFn fn) {
        Registry().push_back({std::move(test_name), std::move(fn)});
    }
};

}  // namespace hermes::tests

#define HERMES_TEST(name)                                                      \
    void name();                                                               \
    static ::hermes::tests::Registrar hermes_test_registrar_##name(#name, name); \
    void name()

#define HERMES_CHECK(condition)                                                \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::ostringstream hermes_test_stream;                             \
            hermes_test_stream << "Check failed: " #condition                  \
                                << " at " << __FILE__ << ':' << __LINE__;      \
            throw std::runtime_error(hermes_test_stream.str());                \
        }                                                                      \
    } while (false)

#define HERMES_CHECK_EQ(left, right)                                           \
    do {                                                                       \
        const auto& hermes_left = (left);                                      \
        const auto& hermes_right = (right);                                    \
        if (!(hermes_left == hermes_right)) {                                  \
            std::ostringstream hermes_test_stream;                             \
            hermes_test_stream << "Check failed: " #left " == " #right         \
                                << " at " << __FILE__ << ':' << __LINE__;      \
            throw std::runtime_error(hermes_test_stream.str());                \
        }                                                                      \
    } while (false)
