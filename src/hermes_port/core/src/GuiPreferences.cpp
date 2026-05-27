#include "hermes/GuiPreferences.h"

#include <algorithm>
#include <initializer_list>
#include <string>

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
    preferences.show_toolbar_tips =
        ReadBoolWithAliases(settings, section, true, {"ShowToolTips", "ToolTips"});
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
    preferences.task_column_layout =
        settings.GetString(section, "HaikuTaskColumnLayout").value_or("");
    preferences.main_split_layout =
        settings.GetString(section, "HaikuMainSplitLayout").value_or("");
    preferences.compose_split_layout =
        settings.GetString(section, "HaikuComposeSplitLayout").value_or("");
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
    settings.SetString(section, "ShowToolTips", preferences.show_toolbar_tips ? "1" : "0");
    settings.SetString(section, "ToolTips", preferences.show_toolbar_tips ? "1" : "0");
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
    settings.SetString(section, "HaikuTaskColumnLayout", preferences.task_column_layout);
    settings.SetString(section, "HaikuMainSplitLayout", preferences.main_split_layout);
    settings.SetString(section, "HaikuComposeSplitLayout", preferences.compose_split_layout);
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
