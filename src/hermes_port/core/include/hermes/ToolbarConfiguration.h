#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace hermes {

struct ToolbarConfiguration {
    std::vector<std::string> visible_entries;
};

bool ToolbarEntryIsSeparator(std::string_view entry);
bool ToolbarConfigurationContains(const ToolbarConfiguration& configuration, std::string_view entry);
std::vector<std::string> HiddenToolbarEntries(const ToolbarConfiguration& configuration,
                                              const std::vector<std::string>& allowed_actions);
bool InsertToolbarEntry(ToolbarConfiguration& configuration,
                        const std::vector<std::string>& allowed_actions,
                        std::string_view entry,
                        std::size_t position);
bool MoveToolbarEntry(ToolbarConfiguration& configuration, std::size_t from, std::size_t to);
bool RemoveToolbarEntry(ToolbarConfiguration& configuration, std::size_t index);
ToolbarConfiguration ResetToolbarConfiguration(const std::vector<std::string>& allowed_actions,
                                               const std::vector<std::string>& default_entries);

ToolbarConfiguration ParseToolbarConfiguration(std::string_view serialized,
                                               const std::vector<std::string>& allowed_actions,
                                               const std::vector<std::string>& default_entries);

std::string SerializeToolbarConfiguration(const ToolbarConfiguration& configuration);

}  // namespace hermes
