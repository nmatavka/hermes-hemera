#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

class SettingsStore {
public:
    virtual ~SettingsStore() = default;

    virtual bool LoadFromFile(const std::filesystem::path& path, std::string* error_message = nullptr) = 0;
    virtual bool SaveToFile(const std::filesystem::path& path, std::string* error_message = nullptr) const = 0;
    virtual bool HasValue(std::string_view section, std::string_view key) const = 0;
    virtual std::optional<std::string> GetString(std::string_view section, std::string_view key) const = 0;
    virtual void SetString(std::string_view section, std::string_view key, std::string_view value) = 0;
    virtual void RemoveValue(std::string_view section, std::string_view key) = 0;
    virtual std::vector<std::string> Sections() const = 0;

    int GetInt(std::string_view section, std::string_view key, int fallback = 0) const;
    bool GetBool(std::string_view section, std::string_view key, bool fallback = false) const;
};

}  // namespace hermes
