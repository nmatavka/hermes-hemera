#include "hermes/ToolbarConfiguration.h"

#include <algorithm>
#include <sstream>

namespace hermes {

namespace {

constexpr std::string_view kSeparator = "-";

std::vector<std::string> Split(std::string_view value) {
    std::vector<std::string> result;
    std::string current;
    for (char ch : value) {
        if (ch == ',') {
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

bool Contains(const std::vector<std::string>& values, std::string_view value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

void AppendEntry(std::vector<std::string>& output,
                 const std::vector<std::string>& allowed_actions,
                 std::string_view entry) {
    if (entry.empty()) {
        return;
    }
    if (ToolbarEntryIsSeparator(entry)) {
        if (output.empty() || ToolbarEntryIsSeparator(output.back())) {
            return;
        }
        output.emplace_back(kSeparator);
        return;
    }
    if (!Contains(allowed_actions, entry) || Contains(output, entry)) {
        return;
    }
    output.emplace_back(entry);
}

}  // namespace

bool ToolbarEntryIsSeparator(std::string_view entry) {
    return entry == kSeparator;
}

bool ToolbarConfigurationContains(const ToolbarConfiguration& configuration, std::string_view entry) {
    return Contains(configuration.visible_entries, entry);
}

std::vector<std::string> HiddenToolbarEntries(const ToolbarConfiguration& configuration,
                                              const std::vector<std::string>& allowed_actions) {
    std::vector<std::string> hidden;
    for (const auto& action : allowed_actions) {
        if (!Contains(configuration.visible_entries, action)) {
            hidden.push_back(action);
        }
    }
    return hidden;
}

bool InsertToolbarEntry(ToolbarConfiguration& configuration,
                        const std::vector<std::string>& allowed_actions,
                        std::string_view entry,
                        std::size_t position) {
    if (entry.empty()) {
        return false;
    }

    std::vector<std::string> updated = configuration.visible_entries;
    position = std::min(position, updated.size());

    if (ToolbarEntryIsSeparator(entry)) {
        if (updated.empty() || position == 0 || position >= updated.size()) {
            return false;
        }
        if (ToolbarEntryIsSeparator(updated[position - 1]) || ToolbarEntryIsSeparator(updated[position])) {
            return false;
        }
        updated.insert(updated.begin() + static_cast<std::ptrdiff_t>(position), std::string(kSeparator));
    } else {
        if (!Contains(allowed_actions, entry) || Contains(updated, entry)) {
            return false;
        }
        updated.insert(updated.begin() + static_cast<std::ptrdiff_t>(position), std::string(entry));
    }

    configuration.visible_entries = std::move(updated);
    return true;
}

bool MoveToolbarEntry(ToolbarConfiguration& configuration, std::size_t from, std::size_t to) {
    if (from >= configuration.visible_entries.size() || to >= configuration.visible_entries.size() || from == to) {
        return false;
    }

    auto entry = configuration.visible_entries[from];
    configuration.visible_entries.erase(configuration.visible_entries.begin() + static_cast<std::ptrdiff_t>(from));
    configuration.visible_entries.insert(configuration.visible_entries.begin() + static_cast<std::ptrdiff_t>(to),
                                         std::move(entry));
    return true;
}

bool RemoveToolbarEntry(ToolbarConfiguration& configuration, std::size_t index) {
    if (index >= configuration.visible_entries.size()) {
        return false;
    }
    configuration.visible_entries.erase(configuration.visible_entries.begin() + static_cast<std::ptrdiff_t>(index));
    while (!configuration.visible_entries.empty() &&
           ToolbarEntryIsSeparator(configuration.visible_entries.front())) {
        configuration.visible_entries.erase(configuration.visible_entries.begin());
    }
    while (!configuration.visible_entries.empty() &&
           ToolbarEntryIsSeparator(configuration.visible_entries.back())) {
        configuration.visible_entries.pop_back();
    }
    for (std::size_t current = 1; current < configuration.visible_entries.size();) {
        if (ToolbarEntryIsSeparator(configuration.visible_entries[current]) &&
            ToolbarEntryIsSeparator(configuration.visible_entries[current - 1])) {
            configuration.visible_entries.erase(configuration.visible_entries.begin() +
                                                static_cast<std::ptrdiff_t>(current));
            continue;
        }
        ++current;
    }
    return true;
}

ToolbarConfiguration ResetToolbarConfiguration(const std::vector<std::string>& allowed_actions,
                                               const std::vector<std::string>& default_entries) {
    return ParseToolbarConfiguration("", allowed_actions, default_entries);
}

ToolbarConfiguration ParseToolbarConfiguration(std::string_view serialized,
                                               const std::vector<std::string>& allowed_actions,
                                               const std::vector<std::string>& default_entries) {
    ToolbarConfiguration configuration;
    const auto parsed_entries = Split(serialized);
    const auto& source_entries = parsed_entries.empty() ? default_entries : parsed_entries;
    for (const auto& entry : source_entries) {
        AppendEntry(configuration.visible_entries, allowed_actions, entry);
    }

    if (!configuration.visible_entries.empty() &&
        ToolbarEntryIsSeparator(configuration.visible_entries.back())) {
        configuration.visible_entries.pop_back();
    }

    if (configuration.visible_entries.empty()) {
        for (const auto& entry : default_entries) {
            AppendEntry(configuration.visible_entries, allowed_actions, entry);
        }
    }

    return configuration;
}

std::string SerializeToolbarConfiguration(const ToolbarConfiguration& configuration) {
    std::ostringstream output;
    bool first = true;
    for (const auto& entry : configuration.visible_entries) {
        if (entry.empty()) {
            continue;
        }
        if (!first) {
            output << ',';
        }
        output << entry;
        first = false;
    }
    return output.str();
}

}  // namespace hermes
