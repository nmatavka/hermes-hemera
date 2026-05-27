#include <exception>
#include <cstdlib>
#include <iostream>

#include "TestRegistry.h"

int main() {
    int failures = 0;
    const char* filter = std::getenv("HERMES_TEST_FILTER");
    for (const auto& test : hermes::tests::Registry()) {
        if (filter != nullptr && *filter != '\0' && test.name != filter) {
            continue;
        }
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& ex) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << ex.what() << '\n';
        }
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }

    std::cout << "All tests passed.\n";
    return 0;
}
