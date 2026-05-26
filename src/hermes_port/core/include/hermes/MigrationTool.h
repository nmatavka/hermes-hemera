#pragma once

#include <filesystem>
#include <string>

namespace hermes {

enum class MigrationSource {
    kOutlook,
    kOutlookExpress,
    kWindowsAddressBook,
    kSimpleMAPI,
};

class MigrationTool {
public:
    virtual ~MigrationTool() = default;

    virtual MigrationSource Source() const = 0;
    virtual bool Import(const std::filesystem::path& source_root,
                        const std::filesystem::path& target_root,
                        std::string* error_message = nullptr) = 0;
};

}  // namespace hermes
