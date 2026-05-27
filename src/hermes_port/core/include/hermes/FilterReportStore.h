#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

struct FilterReportEntry {
    std::string id;
    std::string message_id;
    std::string mailbox_id;
    std::string mailbox_name;
    std::string sender;
    std::string subject;
    std::vector<std::string> matched_rules;
    std::int64_t timestamp = 0;
};

class FilterReportStore {
public:
    virtual ~FilterReportStore() = default;

    virtual bool LoadFromFile(const std::filesystem::path& path, std::string* error_message = nullptr) = 0;
    virtual bool SaveToFile(const std::filesystem::path& path, std::string* error_message = nullptr) const = 0;
    virtual std::vector<FilterReportEntry> Entries() const = 0;
    virtual std::optional<FilterReportEntry> FindByMessageId(std::string_view message_id) const = 0;
    virtual void AddEntry(const FilterReportEntry& entry) = 0;
    virtual void Clear() = 0;
};

class FilesystemFilterReportStore final : public FilterReportStore {
public:
    bool LoadFromFile(const std::filesystem::path& path, std::string* error_message = nullptr) override;
    bool SaveToFile(const std::filesystem::path& path, std::string* error_message = nullptr) const override;
    std::vector<FilterReportEntry> Entries() const override;
    std::optional<FilterReportEntry> FindByMessageId(std::string_view message_id) const override;
    void AddEntry(const FilterReportEntry& entry) override;
    void Clear() override;

private:
    std::vector<FilterReportEntry> entries_;
};

}  // namespace hermes
