#include "hermes/SearchBarSettings.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace hermes {

namespace {

constexpr int kSearchBarMinWidth = 100;
constexpr int kSearchBarMaxWidth = 500;
constexpr int kDefaultSearchBarWidth = 150;
constexpr int kDefaultRecentCount = 5;
constexpr int kMaxRecentCount = 99;
constexpr std::string_view kRecentListSection = "Search Bar Recent Search List";
constexpr std::string_view kJumpUrlDeadHost = "jump.eudora.com/jump.cgi";

std::string TrimWhitespace(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string UrlEncode(std::string_view value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size() * 3);
    for (unsigned char ch : value) {
        const bool unreserved =
            std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~';
        if (unreserved) {
            encoded.push_back(static_cast<char>(ch));
        } else if (ch == ' ') {
            encoded.push_back('+');
        } else {
            encoded.push_back('%');
            encoded.push_back(kHex[(ch >> 4) & 0x0F]);
            encoded.push_back(kHex[ch & 0x0F]);
        }
    }
    return encoded;
}

int ClampSearchBarWidth(int width) {
    if (width < kSearchBarMinWidth) {
        return kSearchBarMinWidth;
    }
    if (width > kSearchBarMaxWidth) {
        return kSearchBarMaxWidth;
    }
    return width;
}

int ClampRecentCount(int count) {
    if (count < 0) {
        return 0;
    }
    if (count > kMaxRecentCount) {
        return kMaxRecentCount;
    }
    return count;
}

std::string RecentTypeKey(int index) {
    return "SearchType" + std::to_string(index);
}

std::string RecentTextKey(int index) {
    return "SearchText" + std::to_string(index);
}

bool HasUsableJumpUrl(std::string_view url) {
    if (url.empty()) {
        return false;
    }
    std::string lowered;
    lowered.reserve(url.size());
    for (char ch : url) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    if (lowered.find("http://") != 0 && lowered.find("https://") != 0) {
        return false;
    }
    return lowered.find(kJumpUrlDeadHost) == std::string::npos;
}

std::string AppendQueryParameter(std::string url,
                                 std::string_view name,
                                 std::string_view value,
                                 bool first_parameter) {
    if (first_parameter) {
        url.push_back('?');
    } else {
        url.push_back('&');
    }
    url.append(name);
    url.push_back('=');
    url.append(UrlEncode(value));
    return url;
}

}  // namespace

std::optional<SearchBarMode> ParseSearchBarMode(int value) {
    switch (value) {
        case static_cast<int>(SearchBarMode::kSearchWeb):
            return SearchBarMode::kSearchWeb;
        case static_cast<int>(SearchBarMode::kSearchAllMailboxes):
            return SearchBarMode::kSearchAllMailboxes;
        case static_cast<int>(SearchBarMode::kSearchCurrentMailbox):
            return SearchBarMode::kSearchCurrentMailbox;
        case static_cast<int>(SearchBarMode::kSearchCurrentFolder):
            return SearchBarMode::kSearchCurrentFolder;
        default:
            return std::nullopt;
    }
}

SearchBarMode SearchBarModeFromSettings(const SettingsStore& settings, std::string_view section) {
    return ParseSearchBarMode(settings.GetInt(
               section, "SearchBarSearchType", static_cast<int>(SearchBarMode::kSearchWeb)))
        .value_or(SearchBarMode::kSearchWeb);
}

int SearchBarWidthFromSettings(const SettingsStore& settings, std::string_view section) {
    return ClampSearchBarWidth(settings.GetInt(section, "SearchBarWidth", kDefaultSearchBarWidth));
}

std::size_t SearchBarMaxRecentEntriesFromSettings(const SettingsStore& settings,
                                                  std::string_view section) {
    return static_cast<std::size_t>(
        ClampRecentCount(settings.GetInt(section, "SearchBarRecentCount", kDefaultRecentCount)));
}

std::vector<SearchBarRecentEntry> SearchBarRecentEntriesFromSettings(const SettingsStore& settings,
                                                                     std::string_view section) {
    std::vector<SearchBarRecentEntry> entries;
    const std::size_t max_entries = SearchBarMaxRecentEntriesFromSettings(settings, section);
    for (std::size_t index = 0; index < max_entries; ++index) {
        const auto mode =
            ParseSearchBarMode(settings.GetInt(kRecentListSection, RecentTypeKey(static_cast<int>(index)), -1));
        if (!mode) {
            break;
        }
        auto text = settings.GetString(kRecentListSection, RecentTextKey(static_cast<int>(index)));
        if (!text) {
            break;
        }
        SearchBarRecentEntry entry;
        entry.mode = *mode;
        entry.text = TrimWhitespace(*text);
        if (entry.text.empty()) {
            continue;
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

void ApplySearchBarSettingsToSettings(const SettingsStore& settings_source,
                                      int width,
                                      SearchBarMode mode,
                                      const std::vector<SearchBarRecentEntry>& recent_entries,
                                      SettingsStore& settings,
                                      std::string_view section) {
    settings.SetString(section, "SearchBarWidth", std::to_string(ClampSearchBarWidth(width)));
    settings.SetString(section, "SearchBarSearchType", std::to_string(static_cast<int>(mode)));

    const std::size_t max_entries = SearchBarMaxRecentEntriesFromSettings(settings_source, section);
    std::size_t index = 0;
    for (const auto& entry : recent_entries) {
        if (entry.text.empty() || index >= max_entries) {
            continue;
        }
        settings.SetString(
            kRecentListSection, RecentTypeKey(static_cast<int>(index)), std::to_string(static_cast<int>(entry.mode)));
        settings.SetString(kRecentListSection, RecentTextKey(static_cast<int>(index)), entry.text);
        ++index;
    }

    while (index < 100) {
        settings.RemoveValue(kRecentListSection, RecentTypeKey(static_cast<int>(index)));
        settings.RemoveValue(kRecentListSection, RecentTextKey(static_cast<int>(index)));
        ++index;
    }
}

void RememberSearchBarRecentEntry(std::vector<SearchBarRecentEntry>* recent_entries,
                                  SearchBarMode mode,
                                  std::string_view text,
                                  std::size_t max_entries) {
    if (recent_entries == nullptr || max_entries == 0) {
        return;
    }

    const std::string trimmed = TrimWhitespace(std::string(text));
    if (trimmed.empty()) {
        return;
    }

    recent_entries->erase(
        std::remove_if(recent_entries->begin(),
                       recent_entries->end(),
                       [&](const SearchBarRecentEntry& entry) {
                           return entry.mode == mode &&
                                  std::equal(trimmed.begin(),
                                             trimmed.end(),
                                             entry.text.begin(),
                                             entry.text.end(),
                                             [](char left, char right) {
                                                 return std::tolower(static_cast<unsigned char>(left)) ==
                                                        std::tolower(static_cast<unsigned char>(right));
                                             }) &&
                                  trimmed.size() == entry.text.size();
                       }),
        recent_entries->end());

    recent_entries->insert(recent_entries->begin(), SearchBarRecentEntry{mode, trimmed});
    if (recent_entries->size() > max_entries) {
        recent_entries->resize(max_entries);
    }
}

std::string SearchBarModeLabel(SearchBarMode mode) {
    switch (mode) {
        case SearchBarMode::kSearchWeb:
            return "Search Web";
        case SearchBarMode::kSearchAllMailboxes:
            return "Search Eudora";
        case SearchBarMode::kSearchCurrentMailbox:
            return "Search Mailbox";
        case SearchBarMode::kSearchCurrentFolder:
            return "Search Mailfolder";
    }
    return "Search Eudora";
}

std::string SearchBarRecentLabel(const SearchBarRecentEntry& entry) {
    return SearchBarModeLabel(entry.mode) + " for \"" + entry.text + "\"";
}

std::string BuildSearchBarWebUrl(const SettingsStore& settings,
                                 std::string_view query,
                                 std::string_view section) {
    const std::string trimmed_query = TrimWhitespace(std::string(query));
    if (trimmed_query.empty()) {
        return {};
    }

    const auto configured = settings.GetString(section, "JumpURL");
    if (configured && HasUsableJumpUrl(*configured)) {
        std::string url = *configured;
        const bool has_query = url.find('?') != std::string::npos;
        url = AppendQueryParameter(std::move(url), "action", "search", !has_query);
        return AppendQueryParameter(std::move(url), "query", trimmed_query, false);
    }

    return "https://duckduckgo.com/?q=" + UrlEncode(trimmed_query);
}

}  // namespace hermes
