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

    virtual bool Discover(const std::filesystem::path& directory, std::string* error_message = nullptr) = 0;
    virtual std::vector<StationeryTemplate> Templates() const = 0;
    virtual std::optional<StationeryTemplate> Find(std::string_view name) const = 0;
};

class FilesystemStationeryStore final : public StationeryStore {
public:
    bool Discover(const std::filesystem::path& directory, std::string* error_message = nullptr) override;
    std::vector<StationeryTemplate> Templates() const override;
    std::optional<StationeryTemplate> Find(std::string_view name) const override;

private:
    static std::optional<StationeryTemplate> ParseTemplate(const std::filesystem::path& path,
                                                           std::string* error_message);
    static std::string Normalize(std::string_view value);

    std::vector<StationeryTemplate> templates_;
};

}  // namespace hermes
