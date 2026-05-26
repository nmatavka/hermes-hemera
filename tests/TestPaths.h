#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace hermes::tests {

inline std::filesystem::path FixtureRoot() {
    return std::filesystem::path(HERMES_FIXTURE_ROOT);
}

inline std::filesystem::path UniqueTempPath(std::string_view prefix) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           (std::string(prefix) + "-" + std::to_string(static_cast<long long>(now)));
}

class ScopedTempDirectory {
public:
    explicit ScopedTempDirectory(std::string_view prefix) : path_(UniqueTempPath(prefix)) {
        std::filesystem::create_directories(path_);
    }

    ~ScopedTempDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    const std::filesystem::path& Path() const {
        return path_;
    }

private:
    std::filesystem::path path_;
};

}  // namespace hermes::tests
