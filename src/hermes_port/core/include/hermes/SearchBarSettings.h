#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/SettingsStore.h"

namespace hermes {

enum class SearchBarMode : int {
    kSearchWeb = 0,
    kSearchAllMailboxes = 1,
    kSearchCurrentMailbox = 2,
    kSearchCurrentFolder = 3,
};

struct SearchBarRecentEntry {
    SearchBarMode mode = SearchBarMode::kSearchAllMailboxes;
    std::string text;
};

std::optional<SearchBarMode> ParseSearchBarMode(int value);
SearchBarMode SearchBarModeFromSettings(const SettingsStore& settings,
                                        std::string_view section = "Settings");
int SearchBarWidthFromSettings(const SettingsStore& settings, std::string_view section = "Settings");
std::size_t SearchBarMaxRecentEntriesFromSettings(const SettingsStore& settings,
                                                  std::string_view section = "Settings");
std::vector<SearchBarRecentEntry> SearchBarRecentEntriesFromSettings(
    const SettingsStore& settings,
    std::string_view section = "Settings");
void ApplySearchBarSettingsToSettings(const SettingsStore& settings_source,
                                      int width,
                                      SearchBarMode mode,
                                      const std::vector<SearchBarRecentEntry>& recent_entries,
                                      SettingsStore& settings,
                                      std::string_view section = "Settings");
void RememberSearchBarRecentEntry(std::vector<SearchBarRecentEntry>* recent_entries,
                                  SearchBarMode mode,
                                  std::string_view text,
                                  std::size_t max_entries);
std::string SearchBarModeLabel(SearchBarMode mode);
std::string SearchBarRecentLabel(const SearchBarRecentEntry& entry);
std::string BuildSearchBarWebUrl(const SettingsStore& settings,
                                 std::string_view query,
                                 std::string_view section = "Settings");

}  // namespace hermes
