#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hermes/SearchBarSettings.h"

namespace hermes {

class SettingsStore;

struct WazooWindowState {
    bool open = true;
    bool restore_on_launch = true;
    int selected_tab = 0;
    std::string frame;
};

struct TocSortDescriptor {
    int primary_field = -1;
    bool primary_descending = false;
    int secondary_field = -1;
    bool secondary_descending = false;
    bool group_by_subject = false;
};

struct GuiPreferences {
    bool show_preview_pane = true;
    bool mark_previewed_read = false;
    int preview_read_seconds = 5;
    bool bring_task_status_to_front = false;
    bool bring_task_error_to_front = true;
    int task_status_state_width = 120;
    int task_status_status_width = 160;
    int task_status_progress_width = 106;
    bool show_toolbar = true;
    bool show_search_bar = true;
    int search_bar_width = 150;
    SearchBarMode search_bar_mode = SearchBarMode::kSearchAllMailboxes;
    std::vector<SearchBarRecentEntry> search_bar_recent_entries;
    std::vector<std::string> recent_mailbox_ids;
    bool show_toolbar_tips = true;
    bool show_toolbar_large_buttons = false;
    bool utility_pane_open = true;
    int utility_pane_height = 220;
    int utility_pane_selected_tab = 0;
    int compose_utility_pane_height = 200;
    int compose_utility_pane_selected_tab = 0;
    std::string main_toolbar_layout;
    std::string compose_toolbar_layout;
    std::string toc_column_layout;
    TocSortDescriptor toc_sort;
    std::string task_column_layout;
    std::string filter_report_column_layout;
    std::string link_history_column_layout;
    std::string search_column_layout;
    std::string plugin_column_layout;
    std::string main_split_layout;
    std::string compose_split_layout;
    std::string nicknames_split_layout;
    bool nicknames_rhs_visible = true;
    int nicknames_selected_tab = 0;
    std::string filters_split_layout;
    std::string directory_services_split_layout;
    bool directory_services_keep_on_top = false;
    std::vector<std::string> directory_services_active_provider_ids;
    std::string file_browser_split_layout;
    std::string search_split_layout;
    std::string help_window_frame;
    std::string help_split_layout;
    std::string help_selected_topic_id;
    std::string import_window_frame;
    std::string import_last_source_root;
    std::string import_selected_settings_snapshot;
    WazooWindowState mailboxes_wazoo;
    WazooWindowState tools_wazoo;
    WazooWindowState tasks_wazoo;
    bool detached_task_status_window_open = false;
    bool detached_task_error_window_open = false;
    std::string detached_task_status_window_frame;
    std::string detached_task_error_window_frame;
    std::string detached_tool_window_layout;
};

GuiPreferences GuiPreferencesFromSettings(const SettingsStore& settings,
                                         std::string_view section = "Settings");

void ApplyGuiPreferencesToSettings(const GuiPreferences& preferences,
                                   SettingsStore& settings,
                                   std::string_view section = "Settings");

}  // namespace hermes
