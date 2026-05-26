#pragma once

#include <memory>

#include "hermes/PluginHost.h"

namespace hermes {

class FilesystemPluginHost final : public PluginHost {
public:
    ~FilesystemPluginHost() override;

    bool Discover(const std::filesystem::path& directory, std::string* error_message = nullptr) override;
    bool LoadPlugin(const std::filesystem::path& path, std::string* error_message = nullptr) override;
    const std::vector<PluginSummary>& Plugins() const override;

private:
    struct LoadedLibrary {
        std::filesystem::path path;
        PluginSummary summary;
        void* handle = nullptr;
    };

    std::vector<LoadedLibrary> libraries_;
    std::vector<PluginSummary> plugins_;
};

}  // namespace hermes
