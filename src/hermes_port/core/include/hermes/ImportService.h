#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace hermes {

enum class ImportArtifactKind {
    kSettingsSnapshot,
    kHelpTopicMap,
    kHelpContents,
    kImportConfig,
    kOther,
};

struct ImportArtifact {
    ImportArtifactKind kind = ImportArtifactKind::kOther;
    std::filesystem::path source_path;
    std::filesystem::path relative_destination;
};

class ImportService {
public:
    virtual ~ImportService() = default;

    virtual std::vector<ImportArtifact> Discover(const std::filesystem::path& source_root) const = 0;
    virtual bool Import(const std::filesystem::path& source_root,
                        const std::filesystem::path& destination_root,
                        std::string* error_message = nullptr) const = 0;
};

class LegacyImportService final : public ImportService {
public:
    std::vector<ImportArtifact> Discover(const std::filesystem::path& source_root) const override;
    bool Import(const std::filesystem::path& source_root,
                const std::filesystem::path& destination_root,
                std::string* error_message = nullptr) const override;
};

}  // namespace hermes
