#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/ComposeMessage.h"

namespace hermes {

struct StationeryTemplate {
    std::string name;
    std::filesystem::path source_path;
    ComposeHeaders headers;
    RichTextDocument body;
    std::string persona;
    std::string signature_name;
};

class StationeryStore {
public:
    virtual ~StationeryStore() = default;

    virtual void SetRootDirectory(std::filesystem::path directory) = 0;
    virtual std::filesystem::path RootDirectory() const = 0;
    virtual bool Discover(const std::filesystem::path& directory, std::string* error_message = nullptr) = 0;
    virtual std::vector<StationeryTemplate> Templates() const = 0;
    virtual std::optional<StationeryTemplate> Find(std::string_view name) const = 0;
    virtual bool SaveTemplate(const StationeryTemplate& stationery, std::string* error_message = nullptr) = 0;
    virtual bool DeleteTemplate(std::string_view name, std::string* error_message = nullptr) = 0;
};

class FilesystemStationeryStore final : public StationeryStore {
public:
    void SetRootDirectory(std::filesystem::path directory) override;
    std::filesystem::path RootDirectory() const override;
    bool Discover(const std::filesystem::path& directory, std::string* error_message = nullptr) override;
    std::vector<StationeryTemplate> Templates() const override;
    std::optional<StationeryTemplate> Find(std::string_view name) const override;
    bool SaveTemplate(const StationeryTemplate& stationery, std::string* error_message = nullptr) override;
    bool DeleteTemplate(std::string_view name, std::string* error_message = nullptr) override;

private:
    static std::optional<StationeryTemplate> ParseTemplate(const std::filesystem::path& path,
                                                           std::string* error_message);
    static std::string Normalize(std::string_view value);

    std::filesystem::path root_directory_;
    std::vector<StationeryTemplate> templates_;
};

}  // namespace hermes
