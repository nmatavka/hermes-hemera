#include "hermes/FilterReportStore.h"

#include <algorithm>

#include "hermes/IniSettingsStore.h"

namespace hermes {

namespace {

std::string SectionName(std::size_t index) {
    return "Entry " + std::to_string(static_cast<unsigned long long>(index));
}

std::string JoinRules(const std::vector<std::string>& rules) {
    std::string joined;
    for (std::size_t index = 0; index < rules.size(); ++index) {
        if (index != 0) {
            joined.push_back(';');
        }
        joined += rules[index];
    }
    return joined;
}

std::vector<std::string> SplitRules(std::string_view rules) {
    std::vector<std::string> result;
    std::string current;
    for (char ch : rules) {
        if (ch == ';') {
            if (!current.empty()) {
                result.push_back(current);
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        result.push_back(current);
    }
    return result;
}

}  // namespace

bool FilesystemFilterReportStore::LoadFromFile(const std::filesystem::path& path, std::string* error_message) {
    IniSettingsStore settings;
    if (!settings.LoadFromFile(path, error_message)) {
        return false;
    }

    entries_.clear();
    for (const auto& section : settings.Sections()) {
        if (section.rfind("Entry ", 0) != 0) {
            continue;
        }
        FilterReportEntry entry;
        entry.id = settings.GetString(section, "Id").value_or(section);
        entry.message_id = settings.GetString(section, "MessageId").value_or("");
        entry.mailbox_id = settings.GetString(section, "MailboxId").value_or("");
        entry.mailbox_name = settings.GetString(section, "MailboxName").value_or("");
        entry.sender = settings.GetString(section, "Sender").value_or("");
        entry.subject = settings.GetString(section, "Subject").value_or("");
        entry.matched_rules =
            SplitRules(settings.GetString(section, "MatchedRules").value_or(""));
        entry.timestamp = static_cast<std::int64_t>(settings.GetInt(section, "Timestamp", 0));
        entries_.push_back(std::move(entry));
    }
    return true;
}

bool FilesystemFilterReportStore::SaveToFile(const std::filesystem::path& path,
                                             std::string* error_message) const {
    IniSettingsStore settings;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const auto section = SectionName(index);
        const auto& entry = entries_[index];
        settings.SetString(section, "Id", entry.id);
        settings.SetString(section, "MessageId", entry.message_id);
        settings.SetString(section, "MailboxId", entry.mailbox_id);
        settings.SetString(section, "MailboxName", entry.mailbox_name);
        settings.SetString(section, "Sender", entry.sender);
        settings.SetString(section, "Subject", entry.subject);
        settings.SetString(section, "MatchedRules", JoinRules(entry.matched_rules));
        settings.SetString(section, "Timestamp", std::to_string(entry.timestamp));
    }
    return settings.SaveToFile(path, error_message);
}

std::vector<FilterReportEntry> FilesystemFilterReportStore::Entries() const {
    return entries_;
}

std::optional<FilterReportEntry> FilesystemFilterReportStore::FindByMessageId(std::string_view message_id) const {
    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const FilterReportEntry& entry) {
        return entry.message_id == message_id;
    });
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return *it;
}

void FilesystemFilterReportStore::AddEntry(const FilterReportEntry& entry) {
    entries_.push_back(entry);
}

void FilesystemFilterReportStore::Clear() {
    entries_.clear();
}

}  // namespace hermes
