#pragma once

#include <string>
#include <string_view>

namespace hermes {

class SettingsStore;

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
    bool show_toolbar_tips = true;
    bool utility_pane_open = true;
    int utility_pane_height = 220;
    int utility_pane_selected_tab = 0;
    int compose_utility_pane_height = 200;
    int compose_utility_pane_selected_tab = 0;
    std::string main_toolbar_layout;
    std::string compose_toolbar_layout;
    std::string toc_column_layout;
    std::string task_column_layout;
    std::string main_split_layout;
    std::string compose_split_layout;
    bool detached_task_status_window_open = false;
    bool detached_task_error_window_open = false;
    std::string detached_task_status_window_frame;
    std::string detached_task_error_window_frame;
};

GuiPreferences GuiPreferencesFromSettings(const SettingsStore& settings,
                                         std::string_view section = "Settings");

void ApplyGuiPreferencesToSettings(const GuiPreferences& preferences,
                                   SettingsStore& settings,
                                   std::string_view section = "Settings");

}  // namespace hermes
