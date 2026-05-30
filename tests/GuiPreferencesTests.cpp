#include <filesystem>

#include "TestRegistry.h"

#include "hermes/GuiPreferences.h"
#include "hermes/IniSettingsStore.h"
#include "hermes/SearchBarSettings.h"

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
    HERMES_CHECK(preferences.show_search_bar);
    HERMES_CHECK_EQ(preferences.search_bar_width, 150);
    HERMES_CHECK(preferences.search_bar_mode == hermes::SearchBarMode::kSearchWeb);
    HERMES_CHECK(preferences.search_bar_recent_entries.empty());
    HERMES_CHECK(preferences.recent_mailbox_ids.empty());
    HERMES_CHECK(preferences.show_toolbar_tips);
    HERMES_CHECK(!preferences.show_toolbar_large_buttons);
    HERMES_CHECK(preferences.bring_task_error_to_front);
    HERMES_CHECK(preferences.bring_task_status_to_front == false);
    HERMES_CHECK_EQ(preferences.preview_read_seconds, 5);
    HERMES_CHECK_EQ(preferences.task_status_state_width, 120);
    HERMES_CHECK_EQ(preferences.task_status_status_width, 160);
    HERMES_CHECK_EQ(preferences.task_status_progress_width, 106);
    HERMES_CHECK(preferences.mailboxes_wazoo.open);
    HERMES_CHECK(preferences.mailboxes_wazoo.restore_on_launch);
    HERMES_CHECK(preferences.tools_wazoo.open);
    HERMES_CHECK(preferences.tasks_wazoo.open);
}

HERMES_TEST(GuiPreferencesReadLegacyProfileSnapshotValues) {
    hermes::IniSettingsStore settings;
    std::string error_message;
    HERMES_CHECK(settings.LoadFromFile(FixtureRoot() / "profile_snapshots" / "Eudora.box", &error_message));

    const hermes::GuiPreferences preferences = hermes::GuiPreferencesFromSettings(settings);
    HERMES_CHECK(preferences.show_toolbar_tips);
    HERMES_CHECK(!preferences.show_toolbar_large_buttons);
    HERMES_CHECK_EQ(preferences.task_status_state_width, 120);
    HERMES_CHECK_EQ(preferences.task_status_status_width, 160);
    HERMES_CHECK_EQ(preferences.task_status_progress_width, 106);
    HERMES_CHECK(preferences.mailboxes_wazoo.open);
    HERMES_CHECK(preferences.tools_wazoo.open);
    HERMES_CHECK(preferences.tasks_wazoo.open);
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
    preferences.show_search_bar = false;
    preferences.search_bar_width = 264;
    preferences.search_bar_mode = hermes::SearchBarMode::kSearchCurrentFolder;
    preferences.search_bar_recent_entries = {
        {hermes::SearchBarMode::kSearchWeb, "retro mac"},
        {hermes::SearchBarMode::kSearchCurrentMailbox, "mailbox only"},
        {hermes::SearchBarMode::kSearchCurrentFolder, "folder scope"},
    };
    preferences.recent_mailbox_ids = {"inbox", "project-alpha", "trash"};
    preferences.show_toolbar_tips = false;
    preferences.show_toolbar_large_buttons = true;
    preferences.utility_pane_open = false;
    preferences.utility_pane_height = 280;
    preferences.utility_pane_selected_tab = 1;
    preferences.compose_utility_pane_height = 222;
    preferences.compose_utility_pane_selected_tab = 1;
    preferences.main_toolbar_layout = "new,check,-,send";
    preferences.compose_toolbar_layout = "save,queue,-,attach";
    preferences.toc_column_layout = "status,priority,from,date,size,subject";
    preferences.toc_sort.primary_field = 10;
    preferences.toc_sort.primary_descending = true;
    preferences.toc_sort.secondary_field = 5;
    preferences.toc_sort.secondary_descending = false;
    preferences.toc_sort.group_by_subject = true;
    preferences.task_column_layout = "task,persona,status,details,progress";
    preferences.filter_report_column_layout = "time,mailbox,sender,subject,rules";
    preferences.link_history_column_layout = "type,title,target,source,launched,time";
    preferences.search_column_layout = "mailbox,subject,sender,score";
    preferences.plugin_column_layout = "name,version,root,path";
    preferences.main_split_layout = "mailbox=0.28;preview=0.55;utility=180";
    preferences.compose_split_layout = "editor=0.74;utility=210";
    preferences.nicknames_split_layout = "list=0.32;detail=0.68";
    preferences.nicknames_rhs_visible = false;
    preferences.nicknames_selected_tab = 2;
    preferences.filters_split_layout = "rules=0.30;editor=0.70";
    preferences.directory_services_split_layout = "providers=0.22;results=0.48;detail=0.30";
    preferences.directory_services_keep_on_top = true;
    preferences.directory_services_active_provider_ids = {"nicknames", "address-book"};
    preferences.file_browser_split_layout = "tree=0.35;detail=0.65";
    preferences.search_split_layout = "results=0.66;detail=0.34";
    preferences.help_window_frame = "90,90,980,760";
    preferences.help_split_layout = "contents=0.28;topics=0.32;detail=0.40";
    preferences.help_selected_topic_id = "HIDD_SETTINGS_SPELL";
    preferences.import_window_frame = "120,120,860,680";
    preferences.import_last_source_root = "/tmp/eudora-import";
    preferences.import_selected_settings_snapshot = "Eudora.box";
    preferences.mailboxes_wazoo.open = true;
    preferences.mailboxes_wazoo.restore_on_launch = true;
    preferences.mailboxes_wazoo.selected_tab = 3;
    preferences.mailboxes_wazoo.frame = "80,80,460,780";
    preferences.tools_wazoo.open = true;
    preferences.tools_wazoo.restore_on_launch = false;
    preferences.tools_wazoo.selected_tab = 1;
    preferences.tools_wazoo.frame = "500,80,920,700";
    preferences.tasks_wazoo.open = false;
    preferences.tasks_wazoo.restore_on_launch = true;
    preferences.tasks_wazoo.selected_tab = 0;
    preferences.tasks_wazoo.frame = "940,80,1280,420";
    preferences.detached_task_status_window_open = true;
    preferences.detached_task_error_window_open = true;
    preferences.detached_task_status_window_frame = "100,100,540,320";
    preferences.detached_task_error_window_frame = "110,140,620,420";
    preferences.detached_tool_window_layout = "mailboxes=40,40,320,720;signatures=120,120,480,500";

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
    HERMES_CHECK(reloaded.show_search_bar == false);
    HERMES_CHECK_EQ(reloaded.search_bar_width, 264);
    HERMES_CHECK(reloaded.search_bar_mode == hermes::SearchBarMode::kSearchCurrentFolder);
    HERMES_CHECK_EQ(reloaded.search_bar_recent_entries.size(), static_cast<std::size_t>(3));
    HERMES_CHECK(reloaded.search_bar_recent_entries[0].mode == hermes::SearchBarMode::kSearchWeb);
    HERMES_CHECK_EQ(reloaded.search_bar_recent_entries[0].text, std::string("retro mac"));
    HERMES_CHECK(reloaded.search_bar_recent_entries[2].mode == hermes::SearchBarMode::kSearchCurrentFolder);
    HERMES_CHECK_EQ(reloaded.search_bar_recent_entries[2].text, std::string("folder scope"));
    HERMES_CHECK_EQ(reloaded.recent_mailbox_ids.size(), static_cast<std::size_t>(3));
    HERMES_CHECK_EQ(reloaded.recent_mailbox_ids[0], std::string("inbox"));
    HERMES_CHECK_EQ(reloaded.recent_mailbox_ids[1], std::string("project-alpha"));
    HERMES_CHECK_EQ(reloaded.recent_mailbox_ids[2], std::string("trash"));
    HERMES_CHECK(reloaded.show_toolbar_tips == false);
    HERMES_CHECK(reloaded.show_toolbar_large_buttons);
    HERMES_CHECK(reloaded.utility_pane_open == false);
    HERMES_CHECK_EQ(reloaded.utility_pane_height, 280);
    HERMES_CHECK_EQ(reloaded.utility_pane_selected_tab, 1);
    HERMES_CHECK_EQ(reloaded.compose_utility_pane_height, 222);
    HERMES_CHECK_EQ(reloaded.compose_utility_pane_selected_tab, 1);
    HERMES_CHECK_EQ(reloaded.main_toolbar_layout, std::string("new,check,-,send"));
    HERMES_CHECK_EQ(reloaded.compose_toolbar_layout, std::string("save,queue,-,attach"));
    HERMES_CHECK_EQ(reloaded.toc_column_layout, std::string("status,priority,from,date,size,subject"));
    HERMES_CHECK_EQ(reloaded.toc_sort.primary_field, 10);
    HERMES_CHECK(reloaded.toc_sort.primary_descending);
    HERMES_CHECK_EQ(reloaded.toc_sort.secondary_field, 5);
    HERMES_CHECK(!reloaded.toc_sort.secondary_descending);
    HERMES_CHECK(reloaded.toc_sort.group_by_subject);
    HERMES_CHECK_EQ(reloaded.task_column_layout, std::string("task,persona,status,details,progress"));
    HERMES_CHECK_EQ(reloaded.filter_report_column_layout, std::string("time,mailbox,sender,subject,rules"));
    HERMES_CHECK_EQ(reloaded.link_history_column_layout,
                    std::string("type,title,target,source,launched,time"));
    HERMES_CHECK_EQ(reloaded.search_column_layout, std::string("mailbox,subject,sender,score"));
    HERMES_CHECK_EQ(reloaded.plugin_column_layout, std::string("name,version,root,path"));
    HERMES_CHECK_EQ(reloaded.main_split_layout, std::string("mailbox=0.28;preview=0.55;utility=180"));
    HERMES_CHECK_EQ(reloaded.compose_split_layout, std::string("editor=0.74;utility=210"));
    HERMES_CHECK_EQ(reloaded.nicknames_split_layout, std::string("list=0.32;detail=0.68"));
    HERMES_CHECK(!reloaded.nicknames_rhs_visible);
    HERMES_CHECK_EQ(reloaded.nicknames_selected_tab, 2);
    HERMES_CHECK_EQ(reloaded.filters_split_layout, std::string("rules=0.30;editor=0.70"));
    HERMES_CHECK_EQ(reloaded.directory_services_split_layout,
                    std::string("providers=0.22;results=0.48;detail=0.30"));
    HERMES_CHECK(reloaded.directory_services_keep_on_top);
    HERMES_CHECK_EQ(reloaded.directory_services_active_provider_ids.size(), static_cast<std::size_t>(2));
    HERMES_CHECK_EQ(reloaded.directory_services_active_provider_ids[0], std::string("nicknames"));
    HERMES_CHECK_EQ(reloaded.directory_services_active_provider_ids[1], std::string("address-book"));
    HERMES_CHECK_EQ(reloaded.file_browser_split_layout, std::string("tree=0.35;detail=0.65"));
    HERMES_CHECK_EQ(reloaded.search_split_layout, std::string("results=0.66;detail=0.34"));
    HERMES_CHECK_EQ(reloaded.help_window_frame, std::string("90,90,980,760"));
    HERMES_CHECK_EQ(reloaded.help_split_layout, std::string("contents=0.28;topics=0.32;detail=0.40"));
    HERMES_CHECK_EQ(reloaded.help_selected_topic_id, std::string("HIDD_SETTINGS_SPELL"));
    HERMES_CHECK_EQ(reloaded.import_window_frame, std::string("120,120,860,680"));
    HERMES_CHECK_EQ(reloaded.import_last_source_root, std::string("/tmp/eudora-import"));
    HERMES_CHECK_EQ(reloaded.import_selected_settings_snapshot, std::string("Eudora.box"));
    HERMES_CHECK(reloaded.mailboxes_wazoo.open);
    HERMES_CHECK(reloaded.mailboxes_wazoo.restore_on_launch);
    HERMES_CHECK_EQ(reloaded.mailboxes_wazoo.selected_tab, 3);
    HERMES_CHECK_EQ(reloaded.mailboxes_wazoo.frame, std::string("80,80,460,780"));
    HERMES_CHECK(reloaded.tools_wazoo.open);
    HERMES_CHECK(!reloaded.tools_wazoo.restore_on_launch);
    HERMES_CHECK_EQ(reloaded.tools_wazoo.selected_tab, 1);
    HERMES_CHECK_EQ(reloaded.tools_wazoo.frame, std::string("500,80,920,700"));
    HERMES_CHECK(!reloaded.tasks_wazoo.open);
    HERMES_CHECK(reloaded.tasks_wazoo.restore_on_launch);
    HERMES_CHECK_EQ(reloaded.tasks_wazoo.selected_tab, 0);
    HERMES_CHECK_EQ(reloaded.tasks_wazoo.frame, std::string("940,80,1280,420"));
    HERMES_CHECK(reloaded.detached_task_status_window_open);
    HERMES_CHECK(reloaded.detached_task_error_window_open);
    HERMES_CHECK_EQ(reloaded.detached_task_status_window_frame, std::string("100,100,540,320"));
    HERMES_CHECK_EQ(reloaded.detached_task_error_window_frame, std::string("110,140,620,420"));
    HERMES_CHECK_EQ(reloaded.detached_tool_window_layout,
                    std::string("mailboxes=40,40,320,720;signatures=120,120,480,500"));
}

HERMES_TEST(SearchBarSettingsBuildUsableJumpUrlAndFallback) {
    hermes::IniSettingsStore settings;

    settings.SetString("Settings", "JumpURL", "https://example.test/jump.cgi");
    const std::string jump_url = hermes::BuildSearchBarWebUrl(settings, "retro mac");
    HERMES_CHECK_EQ(jump_url, std::string("https://example.test/jump.cgi?action=search&query=retro+mac"));

    settings.SetString("Settings", "JumpURL", "http://jump.eudora.com/jump.cgi");
    const std::string fallback_url = hermes::BuildSearchBarWebUrl(settings, "retro mac");
    HERMES_CHECK_EQ(fallback_url, std::string("https://duckduckgo.com/?q=retro+mac"));
}

HERMES_TEST(SearchBarSettingsRememberRecentsDeduplicatesAndCaps) {
    std::vector<hermes::SearchBarRecentEntry> entries = {
        {hermes::SearchBarMode::kSearchAllMailboxes, "Inbox"},
        {hermes::SearchBarMode::kSearchCurrentMailbox, "Sent"},
    };

    hermes::RememberSearchBarRecentEntry(
        &entries, hermes::SearchBarMode::kSearchAllMailboxes, " inbox ", 2);
    HERMES_CHECK_EQ(entries.size(), static_cast<std::size_t>(2));
    HERMES_CHECK(entries[0].mode == hermes::SearchBarMode::kSearchAllMailboxes);
    HERMES_CHECK_EQ(entries[0].text, std::string("inbox"));
    HERMES_CHECK(entries[1].mode == hermes::SearchBarMode::kSearchCurrentMailbox);

    hermes::RememberSearchBarRecentEntry(
        &entries, hermes::SearchBarMode::kSearchWeb, "retro mac", 2);
    HERMES_CHECK_EQ(entries.size(), static_cast<std::size_t>(2));
    HERMES_CHECK(entries[0].mode == hermes::SearchBarMode::kSearchWeb);
    HERMES_CHECK_EQ(entries[0].text, std::string("retro mac"));
}

HERMES_TEST(SearchBarSettingsUseLegacyModeAndRecentBounds) {
    hermes::IniSettingsStore settings;

    HERMES_CHECK(hermes::SearchBarModeFromSettings(settings) == hermes::SearchBarMode::kSearchWeb);
    HERMES_CHECK_EQ(hermes::SearchBarMaxRecentEntriesFromSettings(settings), static_cast<std::size_t>(5));

    settings.SetString("Settings", "SearchBarRecentCount", "200");
    HERMES_CHECK_EQ(hermes::SearchBarMaxRecentEntriesFromSettings(settings), static_cast<std::size_t>(99));

    settings.SetString("Settings", "SearchBarRecentCount", "-12");
    HERMES_CHECK_EQ(hermes::SearchBarMaxRecentEntriesFromSettings(settings), static_cast<std::size_t>(0));
}
