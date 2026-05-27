#include "hermes/FilterStore.h"

#include <algorithm>
#include <cctype>

#include "hermes/IniSettingsStore.h"

namespace hermes {

namespace {

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string NormalizeValue(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return Trim(normalized);
}

std::string FieldToString(FilterField field) {
    switch (field) {
        case FilterField::kFrom:
            return "from";
        case FilterField::kTo:
            return "to";
        case FilterField::kSubject:
            return "subject";
        case FilterField::kBody:
            return "body";
    }
    return "subject";
}

FilterField FieldFromString(std::string_view value) {
    const std::string normalized = NormalizeValue(value);
    if (normalized == "from") {
        return FilterField::kFrom;
    }
    if (normalized == "to") {
        return FilterField::kTo;
    }
    if (normalized == "body") {
        return FilterField::kBody;
    }
    return FilterField::kSubject;
}

std::string OperationToString(FilterOperation operation) {
    switch (operation) {
        case FilterOperation::kContains:
            return "contains";
        case FilterOperation::kEquals:
            return "equals";
        case FilterOperation::kNotContains:
            return "not-contains";
    }
    return "contains";
}

FilterOperation OperationFromString(std::string_view value) {
    const std::string normalized = NormalizeValue(value);
    if (normalized == "equals") {
        return FilterOperation::kEquals;
    }
    if (normalized == "not-contains") {
        return FilterOperation::kNotContains;
    }
    return FilterOperation::kContains;
}

std::string SectionName(std::string_view name) {
    return "Filter " + std::string(name);
}

}  // namespace

bool FilesystemFilterStore::LoadFromFile(const std::filesystem::path& path, std::string* error_message) {
    IniSettingsStore settings;
    if (!settings.LoadFromFile(path, error_message)) {
        return false;
    }

    rules_.clear();
    for (const auto& section : settings.Sections()) {
        const std::string normalized = Normalize(section);
        if (normalized.rfind("filter ", 0) != 0) {
            continue;
        }

        FilterRule rule;
        rule.name = Trim(section.substr(7));
        rule.field = FieldFromString(settings.GetString(section, "Field").value_or("subject"));
        rule.operation =
            OperationFromString(settings.GetString(section, "Operation").value_or("contains"));
        rule.value = settings.GetString(section, "Value").value_or("");
        if (const auto mailbox = settings.GetString(section, "DestinationMailbox")) {
            if (!mailbox->empty()) {
                rule.destination_mailbox = *mailbox;
            }
        }
        rule.mark_as_read = settings.GetBool(section, "MarkAsRead", false);
        rule.mark_as_junk = settings.GetBool(section, "MarkAsJunk", false);
        rule.stop_processing = settings.GetBool(section, "StopProcessing", true);
        if (!rule.name.empty()) {
            rules_.push_back(std::move(rule));
        }
    }

    return true;
}

bool FilesystemFilterStore::SaveToFile(const std::filesystem::path& path, std::string* error_message) const {
    IniSettingsStore settings;
    for (const auto& rule : rules_) {
        const auto section = SectionName(rule.name);
        settings.SetString(section, "Field", FieldToString(rule.field));
        settings.SetString(section, "Operation", OperationToString(rule.operation));
        settings.SetString(section, "Value", rule.value);
        settings.SetString(section,
                           "DestinationMailbox",
                           rule.destination_mailbox ? *rule.destination_mailbox : "");
        settings.SetString(section, "MarkAsRead", rule.mark_as_read ? "1" : "0");
        settings.SetString(section, "MarkAsJunk", rule.mark_as_junk ? "1" : "0");
        settings.SetString(section, "StopProcessing", rule.stop_processing ? "1" : "0");
    }
    return settings.SaveToFile(path, error_message);
}

std::vector<FilterRule> FilesystemFilterStore::Rules() const {
    return rules_;
}

std::optional<FilterRule> FilesystemFilterStore::FindRule(std::string_view name) const {
    const std::string normalized = Normalize(name);
    for (const auto& rule : rules_) {
        if (Normalize(rule.name) == normalized) {
            return rule;
        }
    }
    return std::nullopt;
}

void FilesystemFilterStore::SetRules(std::vector<FilterRule> rules) {
    rules_ = std::move(rules);
}

void FilesystemFilterStore::AddOrReplace(const FilterRule& rule) {
    const std::string normalized = Normalize(rule.name);
    for (auto& existing : rules_) {
        if (Normalize(existing.name) == normalized) {
            existing = rule;
            return;
        }
    }
    rules_.push_back(rule);
}

bool FilesystemFilterStore::Remove(std::string_view name) {
    const std::string normalized = Normalize(name);
    const auto it = std::remove_if(rules_.begin(), rules_.end(), [&](const FilterRule& rule) {
        return Normalize(rule.name) == normalized;
    });
    if (it == rules_.end()) {
        return false;
    }
    rules_.erase(it, rules_.end());
    return true;
}

std::string FilesystemFilterStore::Normalize(std::string_view value) {
    return NormalizeValue(value);
}

}  // namespace hermes
