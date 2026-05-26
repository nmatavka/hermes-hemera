#pragma once

#include <map>

#include "hermes/SettingsStore.h"

namespace hermes {

class IniSettingsStore final : public SettingsStore {
public:
    bool LoadFromFile(const std::filesystem::path& path, std::string* error_message = nullptr) override;
    bool SaveToFile(const std::filesystem::path& path, std::string* error_message = nullptr) const override;
    bool HasValue(std::string_view section, std::string_view key) const override;
    std::optional<std::string> GetString(std::string_view section, std::string_view key) const override;
    void SetString(std::string_view section, std::string_view key, std::string_view value) override;
    void RemoveValue(std::string_view section, std::string_view key) override;
    std::vector<std::string> Sections() const override;

private:
    struct SettingRecord {
        std::string key;
        std::string value;
    };

    struct SectionRecord {
        std::string name;
        std::map<std::string, SettingRecord> values;
    };

    static std::string Normalize(std::string_view value);

    std::map<std::string, SectionRecord> sections_;
};

}  // namespace hermes
