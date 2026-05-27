#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace hermes {

struct ToolbarConfiguration {
    std::vector<std::string> visible_entries;
};

bool ToolbarEntryIsSeparator(std::string_view entry);

ToolbarConfiguration ParseToolbarConfiguration(std::string_view serialized,
                                               const std::vector<std::string>& allowed_actions,
                                               const std::vector<std::string>& default_entries);

std::string SerializeToolbarConfiguration(const ToolbarConfiguration& configuration);

}  // namespace hermes
