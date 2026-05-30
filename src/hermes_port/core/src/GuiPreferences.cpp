#include "hermes/GuiPreferences.h"

#include <algorithm>
#include <initializer_list>
#include <string>

#include "hermes/SearchBarSettings.h"
#include "hermes/SettingsStore.h"

namespace hermes {

namespace {

constexpr int kMinPaneHeight = 96;
constexpr int kDefaultUtilityHeight = 220;
constexpr int kDefaultComposeUtilityHeight = 200;

bool ReadBoolWithAliases(const SettingsStore& settings,
                         std::string_view section,
                         bool fallback,
                         std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        if (settings.HasValue(section, key)) {
            return settings.GetBool(section, key, fallback);
        }
    }
    return fallback;
}

int ReadIntWithAliases(const SettingsStore& settings,
                       std::string_view section,
                       int fallback,
                       std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        if (settings.HasValue(section, key)) {
            return settings.GetInt(section, key, fallback);
        }
    }
    return fallback;
}

int ClampPaneHeight(int height, int fallback) {
    if (height < kMinPaneHeight) {
        return fallback;
    }
    return height;
}

int ClampTabSelection(int selection) {
    return std::max(0, selection);
}

std::vector<std::string> ParseStringList(std::string_view serialized) {
    std::vector<std::string> values;
    std::size_t start = 0;
    while (start <= serialized.size()) {
        const std::size_t end = serialized.find(';', start);
        const std::string_view token =
            end == std::string_view::npos ? serialized.substr(start) : serialized.substr(start, end - start);
        if (!token.empty()) {
            values.emplace_back(token);
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return values;
}

std::string SerializeStringList(const std::vector<std::string>& values) {
    std::string serialized;
    for (const auto& value : values) {
        if (value.empty()) {
            continue;
        }
        if (!serialized.empty()) {
            serialized.push_back(';');
        }
        serialized += value;
    }
    return serialized;
}

WazooWindowState ReadWazooWindowState(const SettingsStore& settings,
                                      std::string_view section,
                                      std::string_view prefix,
                                      bool fallback_open) {
    WazooWindowState state;
    state.open = settings.GetBool(section, std::string(prefix).append("Open"), fallback_open);
    state.restore_on_launch =
        settings.GetBool(section, std::string(prefix).append("Restore"), fallback_open);
    state.selected_tab =
        ClampTabSelection(settings.GetInt(section, std::string(prefix).append("SelectedTab"), 0));
    state.frame = settings.GetString(section, std::string(prefix).append("Frame")).value_or("");
    return state;
}

void WriteWazooWindowState(const WazooWindowState& state,
                           SettingsStore& settings,
                           std::string_view section,
                           std::string_view prefix) {
    settings.SetString(section, std::string(prefix).append("Open"), state.open ? "1" : "0");
    settings.SetString(
        section, std::string(prefix).append("Restore"), state.restore_on_launch ? "1" : "0");
    settings.SetString(
        section, std::string(prefix).append("SelectedTab"), std::to_string(state.selected_tab));
    settings.SetString(section, std::string(prefix).append("Frame"), state.frame);
}

}  // namespace

GuiPreferences GuiPreferencesFromSettings(const SettingsStore& settings, std::string_view section) {
    GuiPreferences preferences;
    preferences.show_preview_pane =
        ReadBoolWithAliases(settings, section, true, {"MailboxPreviewPane"});
    preferences.mark_previewed_read =
        ReadBoolWithAliases(settings, section, false, {"SetPreviewRead"});
    preferences.preview_read_seconds =
        std::max(0, ReadIntWithAliases(settings, section, 5, {"SetPreviewReadSeconds"}));
    preferences.bring_task_status_to_front =
        ReadBoolWithAliases(settings, section, false, {"TaskStatusBringToFront"});
    preferences.bring_task_error_to_front =
        ReadBoolWithAliases(settings, section, true, {"TaskErrorBringToFront"});
    preferences.task_status_state_width =
        std::max(72, ReadIntWithAliases(settings, section, 120, {"TaskStatusStateWidth"}));
    preferences.task_status_status_width =
        std::max(96, ReadIntWithAliases(settings, section, 160, {"TaskStatusStatusWidth"}));
    preferences.task_status_progress_width =
        std::max(84, ReadIntWithAliases(settings, section, 106, {"TaskStatusProgressWidth"}));
    preferences.show_toolbar =
        ReadBoolWithAliases(settings, section, true, {"ShowCoolBars", "ShowCoolBar"});
    preferences.show_search_bar =
        ReadBoolWithAliases(settings, section, true, {"ShowSearchBar"});
    preferences.search_bar_width = SearchBarWidthFromSettings(settings, section);
    preferences.search_bar_mode = SearchBarModeFromSettings(settings, section);
    preferences.search_bar_recent_entries = SearchBarRecentEntriesFromSettings(settings, section);
    preferences.recent_mailbox_ids =
        ParseStringList(settings.GetString(section, "HaikuRecentMailboxIds").value_or(""));
    preferences.show_toolbar_tips =
        ReadBoolWithAliases(settings, section, true, {"ShowToolTips", "ToolTips"});
    preferences.show_toolbar_large_buttons =
        ReadBoolWithAliases(settings, section, false, {"ShowLargeButtons"});
    preferences.utility_pane_open =
        ReadBoolWithAliases(settings, section, true, {"HaikuUtilityPaneOpen"});
    preferences.utility_pane_height = ClampPaneHeight(
        ReadIntWithAliases(settings, section, kDefaultUtilityHeight, {"HaikuUtilityPaneHeight"}),
        kDefaultUtilityHeight);
    preferences.utility_pane_selected_tab = ClampTabSelection(
        ReadIntWithAliases(settings, section, 0, {"HaikuUtilityPaneSelectedTab"}));
    preferences.compose_utility_pane_height = ClampPaneHeight(
        ReadIntWithAliases(
            settings, section, kDefaultComposeUtilityHeight, {"HaikuComposeUtilityPaneHeight"}),
        kDefaultComposeUtilityHeight);
    preferences.compose_utility_pane_selected_tab = ClampTabSelection(
        ReadIntWithAliases(settings, section, 0, {"HaikuComposeUtilityPaneSelectedTab"}));
    preferences.main_toolbar_layout =
        settings.GetString(section, "HaikuMainToolbarLayout").value_or("");
    preferences.compose_toolbar_layout =
        settings.GetString(section, "HaikuComposeToolbarLayout").value_or("");
    preferences.toc_column_layout =
        settings.GetString(section, "HaikuTocColumnLayout").value_or("");
    preferences.toc_sort.primary_field =
        settings.GetInt(section, "HaikuTocPrimarySortField", -1);
    preferences.toc_sort.primary_descending =
        settings.GetBool(section, "HaikuTocPrimarySortDescending", false);
    preferences.toc_sort.secondary_field =
        settings.GetInt(section, "HaikuTocSecondarySortField", -1);
    preferences.toc_sort.secondary_descending =
        settings.GetBool(section, "HaikuTocSecondarySortDescending", false);
    preferences.toc_sort.group_by_subject =
        settings.GetBool(section, "HaikuTocGroupBySubject", false);
    preferences.task_column_layout =
        settings.GetString(section, "HaikuTaskColumnLayout").value_or("");
    preferences.filter_report_column_layout =
        settings.GetString(section, "HaikuFilterReportColumnLayout").value_or("");
    preferences.link_history_column_layout =
        settings.GetString(section, "HaikuLinkHistoryColumnLayout").value_or("");
    preferences.search_column_layout =
        settings.GetString(section, "HaikuSearchColumnLayout").value_or("");
    preferences.plugin_column_layout =
        settings.GetString(section, "HaikuPluginColumnLayout").value_or("");
    preferences.main_split_layout =
        settings.GetString(section, "HaikuMainSplitLayout").value_or("");
    preferences.compose_split_layout =
        settings.GetString(section, "HaikuComposeSplitLayout").value_or("");
    preferences.nicknames_split_layout =
        settings.GetString(section, "HaikuNicknamesSplitLayout").value_or("");
    preferences.nicknames_rhs_visible =
        settings.GetBool(section, "HaikuNicknamesRhsVisible", true);
    preferences.nicknames_selected_tab =
        ClampTabSelection(settings.GetInt(section, "HaikuNicknamesSelectedTab", 0));
    preferences.filters_split_layout =
        settings.GetString(section, "HaikuFiltersSplitLayout").value_or("");
    preferences.directory_services_split_layout =
        settings.GetString(section, "HaikuDirectoryServicesSplitLayout").value_or("");
    preferences.directory_services_keep_on_top =
        settings.GetBool(section, "HaikuDirectoryServicesKeepOnTop", false);
    preferences.directory_services_active_provider_ids =
        ParseStringList(settings.GetString(section, "HaikuDirectoryServicesActiveProviders").value_or(""));
    preferences.file_browser_split_layout =
        settings.GetString(section, "HaikuFileBrowserSplitLayout").value_or("");
    preferences.search_split_layout =
        settings.GetString(section, "HaikuSearchSplitLayout").value_or("");
    preferences.help_window_frame =
        settings.GetString(section, "HaikuHelpWindowFrame").value_or("");
    preferences.help_split_layout =
        settings.GetString(section, "HaikuHelpSplitLayout").value_or("");
    preferences.help_selected_topic_id =
        settings.GetString(section, "HaikuHelpSelectedTopicId").value_or("");
    preferences.import_window_frame =
        settings.GetString(section, "HaikuImportWindowFrame").value_or("");
    preferences.import_last_source_root =
        settings.GetString(section, "HaikuImportLastSourceRoot").value_or("");
    preferences.import_selected_settings_snapshot =
        settings.GetString(section, "HaikuImportSelectedSettingsSnapshot").value_or("");
    preferences.mailboxes_wazoo =
        ReadWazooWindowState(settings, section, "HaikuMailboxesWazoo", true);
    preferences.tools_wazoo = ReadWazooWindowState(settings, section, "HaikuToolsWazoo", true);
    preferences.tasks_wazoo = ReadWazooWindowState(settings, section, "HaikuTasksWazoo", true);
    preferences.detached_task_status_window_open =
        settings.GetBool(section, "HaikuTaskStatusWindowOpen", false);
    preferences.detached_task_error_window_open =
        settings.GetBool(section, "HaikuTaskErrorWindowOpen", false);
    preferences.detached_task_status_window_frame =
        settings.GetString(section, "HaikuTaskStatusWindowFrame").value_or("");
    preferences.detached_task_error_window_frame =
        settings.GetString(section, "HaikuTaskErrorWindowFrame").value_or("");
    preferences.detached_tool_window_layout =
        settings.GetString(section, "HaikuDetachedToolWindowLayout").value_or("");

    const bool has_tasks_wazoo_open = settings.HasValue(section, "HaikuTasksWazooOpen");
    const bool has_tasks_wazoo_frame = settings.HasValue(section, "HaikuTasksWazooFrame");
    const bool has_detached_task_status_open = settings.HasValue(section, "HaikuTaskStatusWindowOpen");
    const bool has_detached_task_error_open = settings.HasValue(section, "HaikuTaskErrorWindowOpen");
    const bool has_detached_task_status_frame = settings.HasValue(section, "HaikuTaskStatusWindowFrame");

    if (!has_tasks_wazoo_frame && has_detached_task_status_frame && preferences.tasks_wazoo.frame.empty()) {
        preferences.tasks_wazoo.frame = preferences.detached_task_status_window_frame;
    }
    if (!has_tasks_wazoo_open && (has_detached_task_status_open || has_detached_task_error_open)) {
        preferences.tasks_wazoo.open = preferences.detached_task_status_window_open ||
                                       preferences.detached_task_error_window_open;
    }
    return preferences;
}

void ApplyGuiPreferencesToSettings(const GuiPreferences& preferences,
                                   SettingsStore& settings,
                                   std::string_view section) {
    settings.SetString(section, "MailboxPreviewPane", preferences.show_preview_pane ? "1" : "0");
    settings.SetString(section, "SetPreviewRead", preferences.mark_previewed_read ? "1" : "0");
    settings.SetString(section, "SetPreviewReadSeconds", std::to_string(preferences.preview_read_seconds));
    settings.SetString(section,
                       "TaskStatusBringToFront",
                       preferences.bring_task_status_to_front ? "1" : "0");
    settings.SetString(
        section, "TaskErrorBringToFront", preferences.bring_task_error_to_front ? "1" : "0");
    settings.SetString(
        section, "TaskStatusStateWidth", std::to_string(preferences.task_status_state_width));
    settings.SetString(
        section, "TaskStatusStatusWidth", std::to_string(preferences.task_status_status_width));
    settings.SetString(section,
                       "TaskStatusProgressWidth",
                       std::to_string(preferences.task_status_progress_width));
    settings.SetString(section, "ShowCoolBars", preferences.show_toolbar ? "1" : "0");
    settings.SetString(section, "ShowSearchBar", preferences.show_search_bar ? "1" : "0");
    ApplySearchBarSettingsToSettings(settings,
                                     preferences.search_bar_width,
                                     preferences.search_bar_mode,
                                     preferences.search_bar_recent_entries,
                                     settings,
                                     section);
    settings.SetString(
        section, "HaikuRecentMailboxIds", SerializeStringList(preferences.recent_mailbox_ids));
    settings.SetString(section, "ShowToolTips", preferences.show_toolbar_tips ? "1" : "0");
    settings.SetString(section, "ToolTips", preferences.show_toolbar_tips ? "1" : "0");
    settings.SetString(
        section, "ShowLargeButtons", preferences.show_toolbar_large_buttons ? "1" : "0");
    settings.SetString(section, "HaikuUtilityPaneOpen", preferences.utility_pane_open ? "1" : "0");
    settings.SetString(
        section, "HaikuUtilityPaneHeight", std::to_string(preferences.utility_pane_height));
    settings.SetString(section,
                       "HaikuUtilityPaneSelectedTab",
                       std::to_string(preferences.utility_pane_selected_tab));
    settings.SetString(section,
                       "HaikuComposeUtilityPaneHeight",
                       std::to_string(preferences.compose_utility_pane_height));
    settings.SetString(section,
                       "HaikuComposeUtilityPaneSelectedTab",
                       std::to_string(preferences.compose_utility_pane_selected_tab));
    settings.SetString(section, "HaikuMainToolbarLayout", preferences.main_toolbar_layout);
    settings.SetString(section, "HaikuComposeToolbarLayout", preferences.compose_toolbar_layout);
    settings.SetString(section, "HaikuTocColumnLayout", preferences.toc_column_layout);
    settings.SetString(
        section, "HaikuTocPrimarySortField", std::to_string(preferences.toc_sort.primary_field));
    settings.SetString(section,
                       "HaikuTocPrimarySortDescending",
                       preferences.toc_sort.primary_descending ? "1" : "0");
    settings.SetString(section,
                       "HaikuTocSecondarySortField",
                       std::to_string(preferences.toc_sort.secondary_field));
    settings.SetString(section,
                       "HaikuTocSecondarySortDescending",
                       preferences.toc_sort.secondary_descending ? "1" : "0");
    settings.SetString(section,
                       "HaikuTocGroupBySubject",
                       preferences.toc_sort.group_by_subject ? "1" : "0");
    settings.SetString(section, "HaikuTaskColumnLayout", preferences.task_column_layout);
    settings.SetString(section,
                       "HaikuFilterReportColumnLayout",
                       preferences.filter_report_column_layout);
    settings.SetString(section,
                       "HaikuLinkHistoryColumnLayout",
                       preferences.link_history_column_layout);
    settings.SetString(section, "HaikuSearchColumnLayout", preferences.search_column_layout);
    settings.SetString(section, "HaikuPluginColumnLayout", preferences.plugin_column_layout);
    settings.SetString(section, "HaikuMainSplitLayout", preferences.main_split_layout);
    settings.SetString(section, "HaikuComposeSplitLayout", preferences.compose_split_layout);
    settings.SetString(section,
                       "HaikuNicknamesSplitLayout",
                       preferences.nicknames_split_layout);
    settings.SetString(
        section, "HaikuNicknamesRhsVisible", preferences.nicknames_rhs_visible ? "1" : "0");
    settings.SetString(
        section, "HaikuNicknamesSelectedTab", std::to_string(preferences.nicknames_selected_tab));
    settings.SetString(section, "HaikuFiltersSplitLayout", preferences.filters_split_layout);
    settings.SetString(section,
                       "HaikuDirectoryServicesSplitLayout",
                       preferences.directory_services_split_layout);
    settings.SetString(section,
                       "HaikuDirectoryServicesKeepOnTop",
                       preferences.directory_services_keep_on_top ? "1" : "0");
    settings.SetString(section,
                       "HaikuDirectoryServicesActiveProviders",
                       SerializeStringList(preferences.directory_services_active_provider_ids));
    settings.SetString(section,
                       "HaikuFileBrowserSplitLayout",
                       preferences.file_browser_split_layout);
    settings.SetString(section, "HaikuSearchSplitLayout", preferences.search_split_layout);
    settings.SetString(section, "HaikuHelpWindowFrame", preferences.help_window_frame);
    settings.SetString(section, "HaikuHelpSplitLayout", preferences.help_split_layout);
    settings.SetString(section, "HaikuHelpSelectedTopicId", preferences.help_selected_topic_id);
    settings.SetString(section, "HaikuImportWindowFrame", preferences.import_window_frame);
    settings.SetString(section,
                       "HaikuImportLastSourceRoot",
                       preferences.import_last_source_root);
    settings.SetString(section,
                       "HaikuImportSelectedSettingsSnapshot",
                       preferences.import_selected_settings_snapshot);
    WriteWazooWindowState(preferences.mailboxes_wazoo, settings, section, "HaikuMailboxesWazoo");
    WriteWazooWindowState(preferences.tools_wazoo, settings, section, "HaikuToolsWazoo");
    WriteWazooWindowState(preferences.tasks_wazoo, settings, section, "HaikuTasksWazoo");
    settings.SetString(section,
                       "HaikuTaskStatusWindowOpen",
                       preferences.detached_task_status_window_open ? "1" : "0");
    settings.SetString(section,
                       "HaikuTaskErrorWindowOpen",
                       preferences.detached_task_error_window_open ? "1" : "0");
    settings.SetString(section,
                       "HaikuTaskStatusWindowFrame",
                       preferences.detached_task_status_window_frame);
    settings.SetString(section,
                       "HaikuTaskErrorWindowFrame",
                       preferences.detached_task_error_window_frame);
    settings.SetString(section,
                       "HaikuDetachedToolWindowLayout",
                       preferences.detached_tool_window_layout);
}

}  // namespace hermes
