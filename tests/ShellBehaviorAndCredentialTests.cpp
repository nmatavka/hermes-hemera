#include <fstream>
#include <string>

#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/CredentialStore.h"
#include "hermes/IniSettingsStore.h"
#include "hermes/MailTransferSettings.h"
#include "hermes/OAuthSupport.h"
#include "hermes/SelectedTextUrlSettings.h"
#include "hermes/ShellBehaviorSettings.h"

HERMES_TEST(ShellBehaviorSettingsRoundTripToSettingsStore) {
    hermes::IniSettingsStore settings;

    hermes::ShellBehaviorSettings behavior;
    behavior.offline = true;
    behavior.control_arrows = true;
    behavior.alt_arrows = false;
    behavior.backspace_delete = false;
    behavior.reply_ctrl_r_to_all = true;

    hermes::ApplyShellBehaviorSettingsToSettings(behavior, settings);
    const hermes::ShellBehaviorSettings reloaded = hermes::ShellBehaviorSettingsFromSettings(settings);

    HERMES_CHECK(reloaded.offline);
    HERMES_CHECK(reloaded.control_arrows);
    HERMES_CHECK(!reloaded.alt_arrows);
    HERMES_CHECK(!reloaded.backspace_delete);
    HERMES_CHECK(reloaded.reply_ctrl_r_to_all);
}

HERMES_TEST(MailTransferSettingsRoundTripToSettingsStore) {
    hermes::IniSettingsStore settings;

    hermes::MailTransferSettings transfer_settings;
    transfer_settings.immediate_send = false;
    transfer_settings.send_on_check = false;
    transfer_settings.leave_mail_on_server = true;
    transfer_settings.transfer_persona_options =
        hermes::TransferPersonaOptionsMode::kSpecifiedOptions;

    hermes::ApplyMailTransferSettingsToSettings(transfer_settings, settings);
    const hermes::MailTransferSettings reloaded =
        hermes::MailTransferSettingsFromSettings(settings);

    HERMES_CHECK(!reloaded.immediate_send);
    HERMES_CHECK(!reloaded.send_on_check);
    HERMES_CHECK(reloaded.leave_mail_on_server);
    HERMES_CHECK(reloaded.transfer_persona_options ==
                 hermes::TransferPersonaOptionsMode::kSpecifiedOptions);
}

HERMES_TEST(FilesystemCredentialStoreDeleteAndClearOperationsPersist) {
    hermes::tests::ScopedTempDirectory temp("credential-store");
    hermes::FilesystemCredentialStore store(temp.Path());

    std::string error_message;
    HERMES_CHECK(store.SaveCredential("persona-1", hermes::CredentialKind::kIncoming, "in-pass", &error_message));
    HERMES_CHECK(store.SaveCredential("persona-1", hermes::CredentialKind::kOutgoing, "out-pass", &error_message));

    HERMES_CHECK_EQ(store.LoadCredential("persona-1", hermes::CredentialKind::kIncoming).value_or(""), std::string("in-pass"));
    HERMES_CHECK_EQ(store.LoadCredential("persona-1", hermes::CredentialKind::kOutgoing).value_or(""), std::string("out-pass"));

    HERMES_CHECK(store.DeleteCredential("persona-1", hermes::CredentialKind::kIncoming, &error_message));
    HERMES_CHECK(!store.LoadCredential("persona-1", hermes::CredentialKind::kIncoming).has_value());
    HERMES_CHECK_EQ(store.LoadCredential("persona-1", hermes::CredentialKind::kOutgoing).value_or(""), std::string("out-pass"));

    HERMES_CHECK(store.ClearAllCredentials(&error_message));
    HERMES_CHECK(!store.LoadCredential("persona-1", hermes::CredentialKind::kOutgoing).has_value());
}

HERMES_TEST(FilesystemOAuthTokenStoreDeleteTokenRemovesSavedToken) {
    hermes::tests::ScopedTempDirectory temp("oauth-token-store");
    hermes::FilesystemOAuthTokenStore store(temp.Path());

    hermes::OAuthTokenRecord record;
    record.account_id = "persona-1";
    record.refresh_token = "refresh-token";
    record.access_token = "access-token";
    record.expires_at_unix = 123456;
    std::string error_message;
    HERMES_CHECK(store.SaveToken(record, &error_message));
    HERMES_CHECK(store.LoadToken("persona-1").has_value());

    HERMES_CHECK(store.DeleteToken("persona-1", &error_message));
    HERMES_CHECK(!store.LoadToken("persona-1").has_value());
}

HERMES_TEST(HaikuMainWindowSourceUsesCommandForWindowsCtrlAndControlForWindowsAlt) {
    const auto source_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuMainWindow.cpp";
    std::ifstream input(source_path);
    HERMES_CHECK(input.good());
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    HERMES_CHECK(contents.find("AddShortcut('R', B_COMMAND_KEY, new BMessage(kReplyPrimaryShortcutMessage));") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("AddShortcut('E', B_COMMAND_KEY, new BMessage(kSendImmediatelyMessage));") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("AddShortcut(B_UP_ARROW, B_COMMAND_KEY, new BMessage(kPreviousMessageMessage));") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("AddShortcut(B_DOWN_ARROW, B_COMMAND_KEY, new BMessage(kNextMessageMessage));") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("AddShortcut(B_UP_ARROW, B_NO_COMMAND_KEY | B_CONTROL_KEY, new BMessage(kPreviousMessageMessage));") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("AddShortcut(B_DOWN_ARROW, B_NO_COMMAND_KEY | B_CONTROL_KEY, new BMessage(kNextMessageMessage));") !=
                 std::string::npos);

    HERMES_CHECK(contents.find("AddShortcut('R', B_CONTROL_KEY") == std::string::npos);
    HERMES_CHECK(contents.find("AddShortcut('E', B_CONTROL_KEY") == std::string::npos);
    HERMES_CHECK(contents.find("AddShortcut('F', B_CONTROL_KEY") == std::string::npos);
    HERMES_CHECK(contents.find("AddShortcut(B_UP_ARROW, B_OPTION_KEY") == std::string::npos);
    HERMES_CHECK(contents.find("AddShortcut(B_DOWN_ARROW, B_OPTION_KEY") == std::string::npos);
}

HERMES_TEST(HaikuShellSourceExposesSelectedTextUrlsWindowCommandsAndInsertRecipients) {
    const auto main_window_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuMainWindow.cpp";
    std::ifstream main_input(main_window_path);
    HERMES_CHECK(main_input.good());
    const std::string main_contents((std::istreambuf_iterator<char>(main_input)),
                                    std::istreambuf_iterator<char>());

    HERMES_CHECK(main_contents.find("SelectedTextUrlActionsFromSettings(shell_host_.Settings())") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("SelectedTextUrlAcceleratorDigitForSlot(slot)") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("AddShortcut(static_cast<uint32>('0' + *digit), B_COMMAND_KEY, new BMessage(command));") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("new BMenuItem(\"Send to Back\", new BMessage(kWindowSendBehindMessage))") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("new BMenuItem(\"Cascade\", new BMessage(kWindowCascadeMessage))") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("new BMenuItem(\"Close All\", new BMessage(kWindowCloseAllMessage))") !=
                 std::string::npos);

    const auto compose_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuComposeWindow.cpp";
    std::ifstream compose_input(compose_path);
    HERMES_CHECK(compose_input.good());
    const std::string compose_contents((std::istreambuf_iterator<char>(compose_input)),
                                       std::istreambuf_iterator<char>());

    HERMES_CHECK(compose_contents.find("insert_recipients_menu_ = new BMenu(\"Insert Recipients\")") !=
                 std::string::npos);
    HERMES_CHECK(compose_contents.find("case kDynamicInsertRecipientMessage:") != std::string::npos);
    HERMES_CHECK(compose_contents.find("Place the caret in To, Cc, Bcc, or Reply-To first.") !=
                 std::string::npos);
}

HERMES_TEST(HaikuMainWindowSourceUsesSummaryBasedImapEnablement) {
    const auto main_window_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuMainWindow.cpp";
    std::ifstream main_input(main_window_path);
    HERMES_CHECK(main_input.good());
    const std::string contents((std::istreambuf_iterator<char>(main_input)),
                               std::istreambuf_iterator<char>());

    HERMES_CHECK(contents.find("const auto item = shell_host_.Workspace().GetMessage(id);") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("return item && !item->download_complete && item->size == 0;") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("return item && item->attachment_count > 0 &&") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("return item && (item->download_complete || item->size > 0);") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("!item->attachments.empty()") == std::string::npos);
}

HERMES_TEST(HaikuMailboxPaneAndToolbarSourceExposeBroadWindowsParitySurface) {
    const auto wazoo_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuWazooWindow.cpp";
    std::ifstream wazoo_input(wazoo_path);
    HERMES_CHECK(wazoo_input.good());
    const std::string wazoo_contents((std::istreambuf_iterator<char>(wazoo_input)),
                                     std::istreambuf_iterator<char>());

    HERMES_CHECK(wazoo_contents.find("case kMailboxRefreshMessage:") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("case kMailboxResyncMessage:") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("case kMailboxResyncTreeMessage:") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("case kMailboxAutoSyncMessage:") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("case kMailboxShowDeletedMessage:") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("case kMailboxEmptyTrashMessage:") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("case kMailboxTrimJunkMessage:") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("new BMenuItem(\"Delete Old Junk\", new BMessage(kMailboxTrimJunkMessage))") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("bytes[0] == B_DELETE") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("new BMenuItem(\"Resync Tree\", new BMessage(kMailboxResyncTreeMessage))") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("new BMenuItem(\"Auto-Sync\", new BMessage(kMailboxAutoSyncMessage))") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("new BMenuItem(\"Show Deleted\", new BMessage(kMailboxShowDeletedMessage))") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("hermes::NormalizeRecentMailboxIds(") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("new BStringItem(\"Recent\", 0, false)") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("summary_view_->SetText(\"Recent mailboxes.\")") !=
                 std::string::npos);

    const auto shell_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuShellHost.cpp";
    std::ifstream shell_input(shell_path);
    HERMES_CHECK(shell_input.good());
    const std::string shell_contents((std::istreambuf_iterator<char>(shell_input)),
                                     std::istreambuf_iterator<char>());

    HERMES_CHECK(shell_contents.find("RememberRecentMailbox(mailbox_id);") != std::string::npos);
    HERMES_CHECK(shell_contents.find("hermes::RememberRecentMailboxId(") != std::string::npos);
    HERMES_CHECK(shell_contents.find("window->GroupId() == \"mailboxes\"") != std::string::npos);

    const auto main_window_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuMainWindow.cpp";
    std::ifstream main_input(main_window_path);
    HERMES_CHECK(main_input.good());
    const std::string main_contents((std::istreambuf_iterator<char>(main_input)),
                                    std::istreambuf_iterator<char>());

    HERMES_CHECK(main_contents.find("recent_mailboxes_menu_ = new BMenu(\"Recent\");") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("RefreshRecentMailboxMenu();") != std::string::npos);
    HERMES_CHECK(main_contents.find("case kOpenRecentMailboxMessage:") != std::string::npos);
    HERMES_CHECK(main_contents.find("RecentMailboxEntries(") != std::string::npos);

    const auto toolbar_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuToolbarSupport.cpp";
    std::ifstream toolbar_input(toolbar_path);
    HERMES_CHECK(toolbar_input.good());
    const std::string toolbar_contents((std::istreambuf_iterator<char>(toolbar_input)),
                                       std::istreambuf_iterator<char>());

    HERMES_CHECK(toolbar_contents.find("for (const auto& mailbox : shell_host.Workspace().Mailboxes())") !=
                 std::string::npos);
    HERMES_CHECK(toolbar_contents.find("for (const auto& entry : shell_host.Nicknames().Entries())") !=
                 std::string::npos);
    HERMES_CHECK(toolbar_contents.find("for (const auto& stationery : shell_host.Stationery().Templates())") !=
                 std::string::npos);
    HERMES_CHECK(toolbar_contents.find("for (const auto& account : shell_host.Accounts().Accounts())") !=
                 std::string::npos);
    HERMES_CHECK(toolbar_contents.find("for (const auto& plugin : shell_host.Plugins().Plugins())") !=
                 std::string::npos);
    HERMES_CHECK(toolbar_contents.find("tool_tips_box_ = new BCheckBox(\"toolbar-tooltips\", \"Show Tooltips\", nullptr);") !=
                 std::string::npos);
    HERMES_CHECK(toolbar_contents.find("large_buttons_box_ = new BCheckBox(\"toolbar-large-buttons\", \"Large Buttons\", nullptr);") !=
                 std::string::npos);
    HERMES_CHECK(toolbar_contents.find("PopulateAvailableEntries();") != std::string::npos);
}

HERMES_TEST(HaikuMainWindowSourceExposesWindowsSearchBarWorkflow) {
    const auto gui_preferences_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "core" / "src" /
        "GuiPreferences.cpp";
    std::ifstream gui_input(gui_preferences_path);
    HERMES_CHECK(gui_input.good());
    const std::string gui_contents((std::istreambuf_iterator<char>(gui_input)),
                                   std::istreambuf_iterator<char>());

    HERMES_CHECK(gui_contents.find("preferences.show_search_bar =") != std::string::npos);
    HERMES_CHECK(gui_contents.find("ReadBoolWithAliases(settings, section, true, {\"ShowSearchBar\"})") !=
                 std::string::npos);
    HERMES_CHECK(gui_contents.find("settings.SetString(section, \"ShowSearchBar\", preferences.show_search_bar ? \"1\" : \"0\");") !=
                 std::string::npos);
    HERMES_CHECK(gui_contents.find("preferences.search_bar_width = SearchBarWidthFromSettings(settings, section);") !=
                 std::string::npos);
    HERMES_CHECK(gui_contents.find("preferences.search_bar_mode = SearchBarModeFromSettings(settings, section);") !=
                 std::string::npos);
    HERMES_CHECK(gui_contents.find("preferences.search_bar_recent_entries = SearchBarRecentEntriesFromSettings(settings, section);") !=
                 std::string::npos);
    HERMES_CHECK(gui_contents.find("preferences.recent_mailbox_ids =") != std::string::npos);
    HERMES_CHECK(gui_contents.find("settings.SetString(") != std::string::npos);
    HERMES_CHECK(gui_contents.find("\"HaikuRecentMailboxIds\"") != std::string::npos);

    const auto main_window_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuMainWindow.cpp";
    std::ifstream main_input(main_window_path);
    HERMES_CHECK(main_input.good());
    const std::string main_contents((std::istreambuf_iterator<char>(main_input)),
                                    std::istreambuf_iterator<char>());

    HERMES_CHECK(main_contents.find("new BMenuItem(\"Show Search Bar\", new BMessage(kToggleSearchBarMessage))") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("search_bar_scope_menu_->SetLabelFromMarked(true);") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("new BMenuItem(\"Search Web\", new BMessage(kSearchBarScopeChangedMessage))") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("new BMenuItem(\"Search Eudora\", new BMessage(kSearchBarScopeChangedMessage))") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("new BMenuItem(\"Search Mailfolder\", new BMessage(kSearchBarScopeChangedMessage))") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("search_bar_recent_menu_ = new BPopUpMenu(\"Search\")") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("search_bar_query_control_ = new BTextControl(\"main-search-query\", \"\", \"\", nullptr);") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("return SearchBarModeLabel(search_bar_mode_);") != std::string::npos);
    HERMES_CHECK(main_contents.find("BuildSearchBarWebUrl(shell_host_.Settings(), query)") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("RememberSearchBarRecentEntry(&gui_preferences_.search_bar_recent_entries,") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("PersistGuiPreferences();") != std::string::npos);
    HERMES_CHECK(main_contents.find("request.scope = HaikuShellHost::SearchRequest::Scope::kCurrentFolder;") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("shell_host_.QueuePendingSearch(std::move(request));") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("shell_host_.OpenToolWindow(\"search\");") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("constexpr uint32_t kSearchBarActionChosenMessage = 'srac';") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("new BMenuItem(SearchBarModeLabel(mode).c_str(), new BMessage(kSearchBarActionChosenMessage))") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("add_action_item(SearchBarMode::kSearchWeb);") != std::string::npos);
    HERMES_CHECK(main_contents.find("add_action_item(SearchBarMode::kSearchCurrentFolder);") != std::string::npos);
    HERMES_CHECK(main_contents.find("\"(No Recent Searches)\"") == std::string::npos);
    HERMES_CHECK(main_contents.find("const std::string query = SearchBarQueryText();") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("if (!query.empty()) {\n                    ExecuteSearchBarQuery();") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("search_bar_query_control_->TextView()->MakeFocus(true);") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("UpdateSearchBarState();") != std::string::npos);

    const auto wazoo_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuWazooWindow.cpp";
    std::ifstream wazoo_input(wazoo_path);
    HERMES_CHECK(wazoo_input.good());
    const std::string wazoo_contents((std::istreambuf_iterator<char>(wazoo_input)),
                                     std::istreambuf_iterator<char>());
    HERMES_CHECK(wazoo_contents.find("search_scope_ = request->scope;") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("anchor_mailbox_id_ = request->anchor_mailbox_id;") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("MailboxMatchesFolderScope(mailbox, mailboxes_by_id)") !=
                 std::string::npos);

    const auto search_settings_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "core" / "src" /
        "SearchBarSettings.cpp";
    std::ifstream search_input(search_settings_path);
    HERMES_CHECK(search_input.good());
    const std::string search_contents((std::istreambuf_iterator<char>(search_input)),
                                      std::istreambuf_iterator<char>());
    HERMES_CHECK(search_contents.find("constexpr int kDefaultSearchBarWidth = 150;") != std::string::npos);
    HERMES_CHECK(search_contents.find("constexpr int kDefaultRecentCount = 5;") != std::string::npos);
    HERMES_CHECK(search_contents.find("constexpr int kMaxRecentCount = 99;") != std::string::npos);
    HERMES_CHECK(search_contents.find("\"SearchBarSearchType\", static_cast<int>(SearchBarMode::kSearchWeb)") !=
                 std::string::npos);
    HERMES_CHECK(search_contents.find("return \"Search Eudora\";") != std::string::npos);
    HERMES_CHECK(search_contents.find("return \"Search Mailfolder\";") != std::string::npos);
    HERMES_CHECK(search_contents.find("https://duckduckgo.com/?q=") != std::string::npos);
    HERMES_CHECK(search_contents.find("jump.eudora.com/jump.cgi") != std::string::npos);
}

HERMES_TEST(HaikuHemeraBrandingAndIconSourcesAreWiredIntoTheShell) {
    const auto identity_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "core" / "include" /
        "hermes" / "HemeraIdentity.h";
    std::ifstream identity_input(identity_path);
    HERMES_CHECK(identity_input.good());
    const std::string identity_contents((std::istreambuf_iterator<char>(identity_input)),
                                        std::istreambuf_iterator<char>());
    HERMES_CHECK(identity_contents.find("kHemeraProductName = \"Hemera\"") != std::string::npos);
    HERMES_CHECK(identity_contents.find("kHemeraLongProductName = \"HERMES Hemera\"") !=
                 std::string::npos);
    HERMES_CHECK(identity_contents.find("kHemeraAppSignature = \"application/x-vnd.hermes-hemera\"") !=
                 std::string::npos);
    HERMES_CHECK(identity_contents.find("kHemeraStableDataDirectoryName = \"Hemera\"") !=
                 std::string::npos);
    HERMES_CHECK(identity_contents.find("kHemeraComposeBridgeName = \"HemeraCompose\"") !=
                 std::string::npos);

    const auto shell_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuShellHost.cpp";
    std::ifstream shell_input(shell_path);
    HERMES_CHECK(shell_input.good());
    const std::string shell_contents((std::istreambuf_iterator<char>(shell_input)),
                                     std::istreambuf_iterator<char>());
    HERMES_CHECK(shell_contents.find("class HemeraApplication final : public BApplication") !=
                 std::string::npos);
    HERMES_CHECK(shell_contents.find("std::string(hermes::kHemeraStableDataDirectoryName)") !=
                 std::string::npos);

    const auto webkit_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" /
        "HaikuWebKitSupport.cpp";
    std::ifstream webkit_input(webkit_path);
    HERMES_CHECK(webkit_input.good());
    const std::string webkit_contents((std::istreambuf_iterator<char>(webkit_input)),
                                      std::istreambuf_iterator<char>());
    HERMES_CHECK(webkit_contents.find("window.HemeraCompose = {") != std::string::npos);
    HERMES_CHECK(webkit_contents.find("window.HermesCompose") == std::string::npos);

    const auto toolbar_header_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" /
        "HaikuToolbarSupport.h";
    std::ifstream toolbar_header_input(toolbar_header_path);
    HERMES_CHECK(toolbar_header_input.good());
    const std::string toolbar_header_contents((std::istreambuf_iterator<char>(toolbar_header_input)),
                                              std::istreambuf_iterator<char>());
    HERMES_CHECK(toolbar_header_contents.find("std::string icon_id;") != std::string::npos);

    const auto toolbar_cpp_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" /
        "HaikuToolbarSupport.cpp";
    std::ifstream toolbar_cpp_input(toolbar_cpp_path);
    HERMES_CHECK(toolbar_cpp_input.good());
    const std::string toolbar_cpp_contents((std::istreambuf_iterator<char>(toolbar_cpp_input)),
                                           std::istreambuf_iterator<char>());
    HERMES_CHECK(toolbar_cpp_contents.find("#include \"HaikuIconCatalog.h\"") != std::string::npos);
    HERMES_CHECK(toolbar_cpp_contents.find("FallbackIconIdForAction") != std::string::npos);
    HERMES_CHECK(toolbar_cpp_contents.find("FindToolbarIcon(FallbackIconIdForAction(*action), large_buttons)") !=
                 std::string::npos);
    HERMES_CHECK(toolbar_cpp_contents.find("\"mail-new\"") != std::string::npos);
    HERMES_CHECK(toolbar_cpp_contents.find("\"mail-send\"") != std::string::npos);
    HERMES_CHECK(toolbar_cpp_contents.find("\"mail-reply\"") != std::string::npos);
    HERMES_CHECK(toolbar_cpp_contents.find("\"mail-forward\"") != std::string::npos);
    HERMES_CHECK(toolbar_cpp_contents.find("\"search\"") != std::string::npos);
    HERMES_CHECK(toolbar_cpp_contents.find("\"printer\"") != std::string::npos);
    HERMES_CHECK(toolbar_cpp_contents.find("\"file-generic\"") != std::string::npos);

    const auto haiku_cmake_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "CMakeLists.txt";
    std::ifstream haiku_cmake_input(haiku_cmake_path);
    HERMES_CHECK(haiku_cmake_input.good());
    const std::string haiku_cmake_contents((std::istreambuf_iterator<char>(haiku_cmake_input)),
                                           std::istreambuf_iterator<char>());
    HERMES_CHECK(haiku_cmake_contents.find("add_executable(\n    Hemera") != std::string::npos);
    HERMES_CHECK(haiku_cmake_contents.find("Hemera.rdef.in") != std::string::npos);
    HERMES_CHECK(haiku_cmake_contents.find("Hemera.PackageInfo.in") != std::string::npos);
    HERMES_CHECK(haiku_cmake_contents.find("HEMERA_PACKAGE_EXECUTABLE") != std::string::npos);
    HERMES_CHECK(haiku_cmake_contents.find("create -C") != std::string::npos);
}

HERMES_TEST(SelectedTextUrlActionsParseAndFormatFromSettings) {
    hermes::IniSettingsStore settings;
    settings.SetString("Settings", "SelectedTextURL1", "Search Web\thttps://example.test/?q=%s");
    settings.SetString("Settings", "SelectedTextURL2", "Raw\tmailto:test@example.test?subject={selection_raw}");
    settings.SetString("Settings", "SelectedTextURL3", "Fallback\thttps://example.test/lookup/");

    const auto actions = hermes::SelectedTextUrlActionsFromSettings(settings);
    HERMES_CHECK_EQ(actions.size(), static_cast<std::size_t>(3));
    HERMES_CHECK_EQ(actions[0].slot, 1);
    HERMES_CHECK_EQ(actions[0].label, std::string("Search Web"));
    HERMES_CHECK_EQ(actions[0].url_format, std::string("https://example.test/?q=%s"));
    HERMES_CHECK_EQ(hermes::BuildSelectedTextUrl(actions[0], "haiku mail"),
                    std::string("https://example.test/?q=haiku+mail"));
    HERMES_CHECK_EQ(hermes::BuildSelectedTextUrl(actions[1], "Need review"),
                    std::string("mailto:test@example.test?subject=Need review"));
    HERMES_CHECK_EQ(hermes::BuildSelectedTextUrl(actions[2], "Hemera"),
                    std::string("https://example.test/lookup/Hemera"));
    HERMES_CHECK_EQ(hermes::SelectedTextUrlAcceleratorDigitForSlot(1).value_or(0), 7);
    HERMES_CHECK_EQ(hermes::SelectedTextUrlAcceleratorDigitForSlot(2).value_or(0), 8);
    HERMES_CHECK_EQ(hermes::SelectedTextUrlAcceleratorDigitForSlot(3).value_or(0), 9);
    HERMES_CHECK_EQ(hermes::SelectedTextUrlAcceleratorDigitForSlot(4).value_or(0), 2);
    HERMES_CHECK_EQ(hermes::SelectedTextUrlAcceleratorDigitForSlot(7).value_or(0), 5);
    HERMES_CHECK_EQ(hermes::SelectedTextUrlAcceleratorLabelForSlot(1), std::string("Cmd+7"));
    HERMES_CHECK_EQ(hermes::SelectedTextUrlAcceleratorLabelForSlot(7), std::string("Cmd+5"));
}

HERMES_TEST(HaikuShellSourceRoutesDirectoryQueriesComposeCommandsAndPrintFallbacks) {
    const auto shell_host_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuShellHost.h";
    std::ifstream shell_host_input(shell_host_path);
    HERMES_CHECK(shell_host_input.good());
    const std::string shell_host_contents((std::istreambuf_iterator<char>(shell_host_input)),
                                          std::istreambuf_iterator<char>());
    HERMES_CHECK(shell_host_contents.find("void QueuePendingDirectoryQuery(std::string query);") !=
                 std::string::npos);
    HERMES_CHECK(shell_host_contents.find("std::optional<std::string> TakePendingDirectoryQuery();") !=
                 std::string::npos);

    const auto main_window_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuMainWindow.cpp";
    std::ifstream main_input(main_window_path);
    HERMES_CHECK(main_input.good());
    const std::string main_contents((std::istreambuf_iterator<char>(main_input)),
                                    std::istreambuf_iterator<char>());
    HERMES_CHECK(main_contents.find("shell_host_.QueuePendingDirectoryQuery(selected_text);") !=
                 std::string::npos);
    HERMES_CHECK(main_contents.find("shell_host_.SaveOpenWindowLayout(&error_message)") !=
                 std::string::npos);

    const auto compose_host_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "ComposeEditorHost.h";
    std::ifstream compose_host_input(compose_host_path);
    HERMES_CHECK(compose_host_input.good());
    const std::string compose_host_contents((std::istreambuf_iterator<char>(compose_host_input)),
                                            std::istreambuf_iterator<char>());
    HERMES_CHECK(compose_host_contents.find("enum class ComposeEditorCommand") != std::string::npos);
    HERMES_CHECK(compose_host_contents.find("virtual bool SupportsCommand(ComposeEditorCommand command) const = 0;") !=
                 std::string::npos);
    HERMES_CHECK(compose_host_contents.find("virtual ComposeEditorCommandState CommandState(ComposeEditorCommand command) const = 0;") !=
                 std::string::npos);
    HERMES_CHECK(compose_host_contents.find("virtual bool ExecuteCommand(ComposeEditorCommand command) = 0;") !=
                 std::string::npos);
    HERMES_CHECK(compose_host_contents.find("virtual void SetSelectionChangeCallback(std::function<void()> callback) = 0;") !=
                 std::string::npos);
    HERMES_CHECK(compose_host_contents.find("virtual std::optional<ComposeEditorStyleSnapshot> CaptureStyleSnapshot() const = 0;") !=
                 std::string::npos);
    HERMES_CHECK(compose_host_contents.find("virtual bool ApplyStyleSnapshot(const ComposeEditorStyleSnapshot& snapshot) = 0;") !=
                 std::string::npos);

    const auto compose_window_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuComposeWindow.cpp";
    std::ifstream compose_input(compose_window_path);
    HERMES_CHECK(compose_input.good());
    const std::string compose_contents((std::istreambuf_iterator<char>(compose_input)),
                                       std::istreambuf_iterator<char>());
    HERMES_CHECK(compose_contents.find("auto* format_menu = new BMenu(\"Format\");") != std::string::npos);
    HERMES_CHECK(compose_contents.find("HandleEditorCommand(*command);") != std::string::npos);
    HERMES_CHECK(compose_contents.find("RefreshEditorCommandMenus();") != std::string::npos);
    HERMES_CHECK(compose_contents.find("HandleFormatPainterCommand();") != std::string::npos);
    HERMES_CHECK(compose_contents.find("format_painter_armed_") != std::string::npos);
    HERMES_CHECK(compose_contents.find("SetSelectionChangeCallback([this]() { HandleEditorSelectionChanged(); })") !=
                 std::string::npos);
    HERMES_CHECK(compose_contents.find("AddShortcut(B_ESCAPE, B_NO_COMMAND_KEY, new BMessage(kCancelFormatPainterMessage));") !=
                 std::string::npos);
    HERMES_CHECK(compose_contents.find("print_item_ = new BMenuItem(\"Print\", new BMessage(kPrintComposeMessage), 'P');") !=
                 std::string::npos);
    HERMES_CHECK(compose_contents.find("print_one_item_ = new BMenuItem(\"Print One\", new BMessage(kPrintOneComposeMessage));") !=
                 std::string::npos);
    HERMES_CHECK(compose_contents.find("print_preview_item_ =") != std::string::npos);
    HERMES_CHECK(compose_contents.find("HandlePrint(bool preview)") != std::string::npos);
    HERMES_CHECK(compose_contents.find("EnsurePrintArtifacts(&preview_path, &printable_path)") !=
                 std::string::npos);
    HERMES_CHECK(compose_contents.find("surface_ != nullptr ? surface_->Snapshot() : snapshot.body") !=
                 std::string::npos);
    HERMES_CHECK(compose_contents.find("WrapPrintHtmlDocument(") != std::string::npos);
    HERMES_CHECK(compose_contents.find("BuildComposePrintHeaderHtml(snapshot)") != std::string::npos);
    HERMES_CHECK(compose_contents.find("const std::string body_printable_text = hermes::StripHtml(prepared_body.html_fragment);") !=
                 std::string::npos);

    const auto webkit_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuWebKitSupport.cpp";
    std::ifstream webkit_input(webkit_path);
    HERMES_CHECK(webkit_input.good());
    const std::string webkit_contents((std::istreambuf_iterator<char>(webkit_input)),
                                      std::istreambuf_iterator<char>());
    HERMES_CHECK(webkit_contents.find("case ComposeEditorCommand::kBold:") != std::string::npos);
    HERMES_CHECK(webkit_contents.find("return \"bold\";") != std::string::npos);
    HERMES_CHECK(webkit_contents.find("EnsurePrintArtifacts(&preview_path, &printable_path)") !=
                 std::string::npos);
    HERMES_CHECK(webkit_contents.find("return SendPathToPrinter(printable_path);") != std::string::npos);
    HERMES_CHECK(webkit_contents.find("print-preview.html") != std::string::npos);
    HERMES_CHECK(webkit_contents.find("std::string prepared_html = AllHtml();") != std::string::npos);
    HERMES_CHECK(webkit_contents.find("std::string printable_text = hermes::StripHtml(prepared_html);") !=
                 std::string::npos);
    HERMES_CHECK(webkit_contents.find("WriteWholeFile(preview_document_path, prepared_html)") !=
                 std::string::npos);

    const auto wazoo_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "HaikuWazooWindow.cpp";
    std::ifstream wazoo_input(wazoo_path);
    HERMES_CHECK(wazoo_input.good());
    const std::string wazoo_contents((std::istreambuf_iterator<char>(wazoo_input)),
                                     std::istreambuf_iterator<char>());
    HERMES_CHECK(wazoo_contents.find("kDirectoryKeepOnTopMessage") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("Window()->AddCommonFilter(tab_filter_)") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("Window()->SetFeel(enabled ? B_FLOATING_APP_WINDOW_FEEL : B_NORMAL_WINDOW_FEEL)") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("HandleTabNavigation(") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("PrintableDetailText(") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("provider_list_->SetSelectionMode(B_MULTIPLE_SELECTION_LIST);") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("results_view_->SetSelectionMode(B_MULTIPLE_SELECTION_LIST);") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("preferences.directory_services_keep_on_top = keep_on_top_;") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("preferences.directory_services_active_provider_ids = ActiveProviderIds();") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("SearchProviders(active_provider_ids, query)") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("FocusQuery();") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("case kDirectoryQueryModifiedMessage:") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("case kDirectoryResultSelectionMessage:") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("UpdateSummary(query_control_->Text() != nullptr ? query_control_->Text() : \"\");") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("results_view_->Clear();") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("CheckClosePrintPreview();\n        const std::string query = query_control_->Text() != nullptr ? query_control_->Text() : \"\";") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("LongDetailText(active_provider_ids, query, results_)") !=
                 std::string::npos);
    HERMES_CHECK(wazoo_contents.find("HandlePrint(bool preview) override") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("CanPrintPreview() const override") != std::string::npos);
    HERMES_CHECK(wazoo_contents.find("ComposeAddressText(selection)") != std::string::npos);

    const auto paige_path =
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "PaigeEditorView.cpp";
    std::ifstream paige_input(paige_path);
    HERMES_CHECK(paige_input.good());
    const std::string paige_contents((std::istreambuf_iterator<char>(paige_input)),
                                     std::istreambuf_iterator<char>());
    HERMES_CHECK(paige_contents.find("InsertNativeHtmlFragment(") != std::string::npos);
    HERMES_CHECK(paige_contents.find("InsertNativeHorizontalRule(") != std::string::npos);
    HERMES_CHECK(paige_contents.find("ShouldCopyParagraphInfo(") != std::string::npos);
    HERMES_CHECK(paige_contents.find("snapshot.include_paragraph_style = ShouldCopyParagraphInfo(document, selection);") !=
                 std::string::npos);
}
