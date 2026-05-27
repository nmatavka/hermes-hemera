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
