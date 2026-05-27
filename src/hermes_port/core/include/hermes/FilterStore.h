#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/FilterEngine.h"

namespace hermes {

class FilterStore {
public:
    virtual ~FilterStore() = default;

    virtual bool LoadFromFile(const std::filesystem::path& path, std::string* error_message = nullptr) = 0;
    virtual bool SaveToFile(const std::filesystem::path& path, std::string* error_message = nullptr) const = 0;
    virtual std::vector<FilterRule> Rules() const = 0;
    virtual std::optional<FilterRule> FindRule(std::string_view name) const = 0;
    virtual void SetRules(std::vector<FilterRule> rules) = 0;
    virtual void AddOrReplace(const FilterRule& rule) = 0;
    virtual bool Remove(std::string_view name) = 0;
};

class FilesystemFilterStore final : public FilterStore {
public:
    bool LoadFromFile(const std::filesystem::path& path, std::string* error_message = nullptr) override;
    bool SaveToFile(const std::filesystem::path& path, std::string* error_message = nullptr) const override;
    std::vector<FilterRule> Rules() const override;
    std::optional<FilterRule> FindRule(std::string_view name) const override;
    void SetRules(std::vector<FilterRule> rules) override;
    void AddOrReplace(const FilterRule& rule) override;
    bool Remove(std::string_view name) override;

private:
    static std::string Normalize(std::string_view value);

    std::vector<FilterRule> rules_;
};

}  // namespace hermes
