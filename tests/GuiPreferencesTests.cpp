#include <filesystem>

#include "TestRegistry.h"

#include "hermes/GuiPreferences.h"
#include "hermes/IniSettingsStore.h"

namespace {

std::filesystem::path FixtureRoot() {
#ifdef HERMES_FIXTURE_ROOT
    return std::filesystem::path(HERMES_FIXTURE_ROOT);
#else
    return std::filesystem::current_path() / "tests" / "fixtures" / "legacy";
#endif
}

}  // namespace

HERMES_TEST(GuiPreferencesUseLegacyDefaultsWhenUnset) {
    hermes::IniSettingsStore settings;
    const hermes::GuiPreferences preferences = hermes::GuiPreferencesFromSettings(settings);

    HERMES_CHECK(preferences.show_preview_pane);
    HERMES_CHECK(preferences.show_toolbar);
    HERMES_CHECK(preferences.show_toolbar_tips);
    HERMES_CHECK(preferences.bring_task_error_to_front);
    HERMES_CHECK(preferences.bring_task_status_to_front == false);
    HERMES_CHECK_EQ(preferences.preview_read_seconds, 5);
    HERMES_CHECK_EQ(preferences.task_status_state_width, 120);
    HERMES_CHECK_EQ(preferences.task_status_status_width, 160);
    HERMES_CHECK_EQ(preferences.task_status_progress_width, 106);
}

HERMES_TEST(GuiPreferencesReadLegacyProfileSnapshotValues) {
    hermes::IniSettingsStore settings;
    std::string error_message;
    HERMES_CHECK(settings.LoadFromFile(FixtureRoot() / "profile_snapshots" / "Eudora.box", &error_message));

    const hermes::GuiPreferences preferences = hermes::GuiPreferencesFromSettings(settings);
    HERMES_CHECK(preferences.show_toolbar_tips);
    HERMES_CHECK_EQ(preferences.task_status_state_width, 120);
    HERMES_CHECK_EQ(preferences.task_status_status_width, 160);
    HERMES_CHECK_EQ(preferences.task_status_progress_width, 106);
}

HERMES_TEST(GuiPreferencesRoundTripToSettingsStore) {
    hermes::IniSettingsStore settings;

    hermes::GuiPreferences preferences;
    preferences.show_preview_pane = false;
    preferences.mark_previewed_read = true;
    preferences.preview_read_seconds = 9;
    preferences.bring_task_status_to_front = true;
    preferences.bring_task_error_to_front = false;
    preferences.task_status_state_width = 144;
    preferences.task_status_status_width = 188;
    preferences.task_status_progress_width = 132;
    preferences.show_toolbar = false;
    preferences.show_toolbar_tips = false;
    preferences.utility_pane_open = false;
    preferences.utility_pane_height = 280;
    preferences.utility_pane_selected_tab = 1;
    preferences.compose_utility_pane_height = 222;
    preferences.compose_utility_pane_selected_tab = 1;
    preferences.main_toolbar_layout = "new,check,-,send";
    preferences.compose_toolbar_layout = "save,queue,-,attach";
    preferences.toc_column_layout = "status,priority,from,date,size,subject";
    preferences.task_column_layout = "task,persona,status,details,progress";
    preferences.main_split_layout = "mailbox=0.28;preview=0.55;utility=180";
    preferences.compose_split_layout = "editor=0.74;utility=210";
    preferences.detached_task_status_window_open = true;
    preferences.detached_task_error_window_open = true;
    preferences.detached_task_status_window_frame = "100,100,540,320";
    preferences.detached_task_error_window_frame = "110,140,620,420";

    hermes::ApplyGuiPreferencesToSettings(preferences, settings);
    const hermes::GuiPreferences reloaded = hermes::GuiPreferencesFromSettings(settings);

    HERMES_CHECK(reloaded.show_preview_pane == false);
    HERMES_CHECK(reloaded.mark_previewed_read);
    HERMES_CHECK_EQ(reloaded.preview_read_seconds, 9);
    HERMES_CHECK(reloaded.bring_task_status_to_front);
    HERMES_CHECK(reloaded.bring_task_error_to_front == false);
    HERMES_CHECK_EQ(reloaded.task_status_state_width, 144);
    HERMES_CHECK_EQ(reloaded.task_status_status_width, 188);
    HERMES_CHECK_EQ(reloaded.task_status_progress_width, 132);
    HERMES_CHECK(reloaded.show_toolbar == false);
    HERMES_CHECK(reloaded.show_toolbar_tips == false);
    HERMES_CHECK(reloaded.utility_pane_open == false);
    HERMES_CHECK_EQ(reloaded.utility_pane_height, 280);
    HERMES_CHECK_EQ(reloaded.utility_pane_selected_tab, 1);
    HERMES_CHECK_EQ(reloaded.compose_utility_pane_height, 222);
    HERMES_CHECK_EQ(reloaded.compose_utility_pane_selected_tab, 1);
    HERMES_CHECK_EQ(reloaded.main_toolbar_layout, std::string("new,check,-,send"));
    HERMES_CHECK_EQ(reloaded.compose_toolbar_layout, std::string("save,queue,-,attach"));
    HERMES_CHECK_EQ(reloaded.toc_column_layout, std::string("status,priority,from,date,size,subject"));
    HERMES_CHECK_EQ(reloaded.task_column_layout, std::string("task,persona,status,details,progress"));
    HERMES_CHECK_EQ(reloaded.main_split_layout, std::string("mailbox=0.28;preview=0.55;utility=180"));
    HERMES_CHECK_EQ(reloaded.compose_split_layout, std::string("editor=0.74;utility=210"));
    HERMES_CHECK(reloaded.detached_task_status_window_open);
    HERMES_CHECK(reloaded.detached_task_error_window_open);
    HERMES_CHECK_EQ(reloaded.detached_task_status_window_frame, std::string("100,100,540,320"));
    HERMES_CHECK_EQ(reloaded.detached_task_error_window_frame, std::string("110,140,620,420"));
}
