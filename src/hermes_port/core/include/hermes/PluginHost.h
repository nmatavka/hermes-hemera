#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace hermes {

struct PluginSummary {
    std::filesystem::path path;
    std::string identifier;
    std::string display_name;
    std::string version;
    std::uint32_t capabilities = 0;
};

class PluginHost {
public:
    virtual ~PluginHost() = default;

    virtual bool Discover(const std::filesystem::path& directory, std::string* error_message = nullptr) = 0;
    virtual bool LoadPlugin(const std::filesystem::path& path, std::string* error_message = nullptr) = 0;
    virtual const std::vector<PluginSummary>& Plugins() const = 0;
};

}  // namespace hermes
