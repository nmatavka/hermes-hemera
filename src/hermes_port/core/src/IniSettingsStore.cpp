#include "hermes/IniSettingsStore.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

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

}  // namespace

int SettingsStore::GetInt(std::string_view section, std::string_view key, int fallback) const {
    const auto value = GetString(section, key);
    if (!value) {
        return fallback;
    }

    try {
        return std::stoi(*value);
    } catch (...) {
        return fallback;
    }
}

bool SettingsStore::GetBool(std::string_view section, std::string_view key, bool fallback) const {
    const auto value = GetString(section, key);
    if (!value) {
        return fallback;
    }

    const std::string normalized = NormalizeValue(*value);
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    return fallback;
}

bool IniSettingsStore::LoadFromFile(const std::filesystem::path& path, std::string* error_message) {
    std::ifstream input(path);
    if (!input.is_open()) {
        if (error_message) {
            *error_message = "Unable to open INI file: " + path.string();
        }
        return false;
    }

    sections_.clear();
    std::string current_section;
    std::string line;

    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            current_section = Trim(line.substr(1, line.size() - 2));
            const std::string section_key = Normalize(current_section);
            sections_[section_key].name = current_section;
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, separator));
        const std::string value = Trim(line.substr(separator + 1));

        const std::string section_key = Normalize(current_section);
        SectionRecord& section = sections_[section_key];
        if (section.name.empty()) {
            section.name = current_section;
        }
        section.values[Normalize(key)] = SettingRecord{key, value};
    }

    return true;
}

bool IniSettingsStore::SaveToFile(const std::filesystem::path& path, std::string* error_message) const {
    std::ofstream output(path);
    if (!output.is_open()) {
        if (error_message) {
            *error_message = "Unable to write INI file: " + path.string();
        }
        return false;
    }

    bool first_section = true;
    for (const auto& section_pair : sections_) {
        const SectionRecord& section = section_pair.second;
        if (!first_section) {
            output << '\n';
        }
        first_section = false;

        if (!section.name.empty()) {
            output << '[' << section.name << ']' << '\n';
        }

        for (const auto& value_pair : section.values) {
            const SettingRecord& record = value_pair.second;
            output << record.key << '=' << record.value << '\n';
        }
    }

    return true;
}

bool IniSettingsStore::HasValue(std::string_view section, std::string_view key) const {
    return static_cast<bool>(GetString(section, key));
}

std::optional<std::string> IniSettingsStore::GetString(std::string_view section, std::string_view key) const {
    const auto section_it = sections_.find(Normalize(section));
    if (section_it == sections_.end()) {
        return std::nullopt;
    }

    const auto value_it = section_it->second.values.find(Normalize(key));
    if (value_it == section_it->second.values.end()) {
        return std::nullopt;
    }

    return value_it->second.value;
}

void IniSettingsStore::SetString(std::string_view section, std::string_view key, std::string_view value) {
    SectionRecord& section_record = sections_[Normalize(section)];
    if (section_record.name.empty()) {
        section_record.name = std::string(section);
    }

    section_record.values[Normalize(key)] = SettingRecord{std::string(key), std::string(value)};
}

void IniSettingsStore::RemoveValue(std::string_view section, std::string_view key) {
    const auto section_it = sections_.find(Normalize(section));
    if (section_it == sections_.end()) {
        return;
    }

    section_it->second.values.erase(Normalize(key));
    if (section_it->second.values.empty() && section_it->second.name.empty()) {
        sections_.erase(section_it);
    }
}

std::vector<std::string> IniSettingsStore::Sections() const {
    std::vector<std::string> names;
    names.reserve(sections_.size());
    for (const auto& entry : sections_) {
        names.push_back(entry.second.name);
    }
    return names;
}

std::string IniSettingsStore::Normalize(std::string_view value) {
    return NormalizeValue(value);
}

}  // namespace hermes
