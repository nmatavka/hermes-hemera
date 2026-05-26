#include "hermes/ImportService.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <optional>

namespace hermes {

namespace {

std::string ToLower(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (char ch : value) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return lowered;
}

bool HasSettingsSnapshotName(const std::filesystem::path& path) {
    static constexpr std::array<const char*, 7> kExtensions = {".adr", ".box", ".fil", ".mis", ".sas", ".tol",
                                                                ".blk"};
    const std::string extension = ToLower(path.extension().string());
    return ToLower(path.filename().string()).rfind("eudora", 0) == 0 &&
           std::find_if(kExtensions.begin(), kExtensions.end(), [&](const char* candidate) {
               return extension == candidate;
           }) != kExtensions.end();
}

std::optional<ImportArtifact> Classify(const std::filesystem::path& source_path) {
    const std::string filename = source_path.filename().string();
    if (HasSettingsSnapshotName(source_path)) {
        return ImportArtifact{ImportArtifactKind::kSettingsSnapshot,
                              source_path,
                              std::filesystem::path("profile_snapshots") / source_path.filename()};
    }
    if (filename == "WBImport.INI") {
        return ImportArtifact{ImportArtifactKind::kImportConfig,
                              source_path,
                              std::filesystem::path("import") / source_path.filename()};
    }
    if (source_path.extension() == ".hh") {
        return ImportArtifact{ImportArtifactKind::kHelpTopicMap,
                              source_path,
                              std::filesystem::path("help") / source_path.filename()};
    }
    if (source_path.extension() == ".cnt") {
        return ImportArtifact{ImportArtifactKind::kHelpContents,
                              source_path,
                              std::filesystem::path("help") / source_path.filename()};
    }
    return std::nullopt;
}

bool CopyArtifact(const ImportArtifact& artifact,
                  const std::filesystem::path& destination_root,
                  std::string* error_message) {
    const std::filesystem::path destination_path = destination_root / artifact.relative_destination;
    std::error_code create_error;
    std::filesystem::create_directories(destination_path.parent_path(), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create import destination: " + create_error.message();
        }
        return false;
    }

    std::error_code copy_error;
    std::filesystem::copy_file(artifact.source_path,
                               destination_path,
                               std::filesystem::copy_options::overwrite_existing,
                               copy_error);
    if (copy_error) {
        if (error_message) {
            *error_message = "Unable to copy " + artifact.source_path.string() + ": " + copy_error.message();
        }
        return false;
    }
    return true;
}

}  // namespace

std::vector<ImportArtifact> LegacyImportService::Discover(const std::filesystem::path& source_root) const {
    std::vector<ImportArtifact> artifacts;
    if (!std::filesystem::exists(source_root)) {
        return artifacts;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (auto artifact = Classify(entry.path())) {
            artifacts.push_back(std::move(*artifact));
        }
    }
    return artifacts;
}

bool LegacyImportService::Import(const std::filesystem::path& source_root,
                                 const std::filesystem::path& destination_root,
                                 std::string* error_message) const {
    const auto artifacts = Discover(source_root);
    if (artifacts.empty()) {
        if (error_message) {
            *error_message = "No importable legacy artifacts were discovered in " + source_root.string();
        }
        return false;
    }

    for (const auto& artifact : artifacts) {
        if (!CopyArtifact(artifact, destination_root, error_message)) {
            return false;
        }
    }
    return true;
}

}  // namespace hermes
