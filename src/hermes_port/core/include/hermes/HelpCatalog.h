#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

struct HelpTopic {
    std::string id;
    std::string label;
    std::filesystem::path source_path;
};

class HelpCatalog {
public:
    virtual ~HelpCatalog() = default;

    virtual std::vector<HelpTopic> Topics() const = 0;
    virtual std::optional<HelpTopic> FindById(std::string_view id) const = 0;
};

class LegacyHelpCatalog final : public HelpCatalog {
public:
    bool LoadTopicMap(const std::filesystem::path& path, std::string* error_message = nullptr);

    std::vector<HelpTopic> Topics() const override;
    std::optional<HelpTopic> FindById(std::string_view id) const override;

private:
    std::vector<HelpTopic> topics_;
};

}  // namespace hermes
