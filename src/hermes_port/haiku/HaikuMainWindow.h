#pragma once

#include <memory>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <OS.h>
#include <Window.h>

#include "hermes/GuiPreferences.h"
#include "hermes/MailboxSort.h"
#include "hermes/MailboxUiSettings.h"
#include "hermes/MessageRenderer.h"
#include "hermes/MailboxWorkflow.h"
#include "hermes/SelectedTextUrlSettings.h"

class BListView;
class BButton;
class BColumnListView;
class BMenu;
class BMenuField;
class BMenuItem;
class BMessageRunner;
class BStringView;
class BTabView;
class BTextControl;
class BTextView;
class BView;
class BWindow;

namespace hermes {
struct MessageDetail;
}

namespace hemera::haiku {

class HaikuShellHost;
class HaikuToolbarCustomizationWindow;
class HaikuWebKitMessageView;

class HaikuMainWindow final : public BWindow {
public:
    explicit HaikuMainWindow(HaikuShellHost& shell_host);
    ~HaikuMainWindow() override;

    void MessageReceived(BMessage* message) override;
    void MenusBeginning() override;
    bool QuitRequested() override;
    void RefreshWorkspace();
    void SetCurrentMailbox(std::string mailbox_id, bool activate_window);
    bool SelectMessage(std::string message_id, bool activate_window);
    void SaveWindowLayoutState();

private:
    enum class MailboxFindTarget {
        kToc = 0,
        kPreview,
    };

    struct TaskEntryMetadata {
        std::string id;
        bool is_imap_action = false;
        bool retryable = false;
        bool cancelable = false;
    };

    struct TaskErrorEntry {
        std::string summary;
        std::string detail;
    };

    struct SelectedCommandState {
        bool has_selection = false;
        bool all_read = false;
        bool all_unread = false;
        std::optional<hermes::LegacyMessageStatus> unanimous_status;
        std::optional<int> unanimous_label;
        std::optional<int> unanimous_priority;
        std::optional<hermes::PopServerStatus> unanimous_pop_status;
    };

    void PopulateWorkspace();
    void PopulateMessagesForCurrentMailbox();
    void PopulatePreview();
    void PopulatePreviewNow();
    void PopulateAttachments(const hermes::MessageDetail* detail);
    void PopulateTaskStatus();
    void PopulateTaskErrors();
    void PopulatePreviewHeader(const hermes::MessageDetail& detail);
    void PopulatePreviewBody(const hermes::MessageDetail& detail);
    std::optional<hermes::MessageRenderRequest> BuildRenderRequest(const hermes::MessageDetail& detail) const;
    void PopulatePreviewPlaceholder(std::string message);
    void ApplyGuiPreferences();
    void PersistGuiPreferences();
    void RebuildToolbar();
    void UpdateViewMenuMarks();
    void UpdateCommandState();
    void UpdateDynamicCommandLabels();
    void UpdateCheckedCommandState();
    SelectedCommandState ComputeSelectedCommandState() const;
    bool IsCommandEnabled(uint32 command) const;
    void TogglePreviewPane();
    void ToggleToolbar();
    void ToggleSearchBar();
    void ToggleUtilityPane();
    void SelectUtilityTab(int32 index);
    bool SelectRelativeMessage(int delta, bool open_window = false);
    void SetStatusMessage(std::string message);
    void RefreshTaskUtilityFocus(bool tasks_changed, bool errors_changed);
    void SchedulePreviewRead();
    void SchedulePreviewRefresh();
    void MarkSelectedMessageReadFromPreview();
    void CancelPreviewRead();
    void CancelPreviewRefresh();
    void SchedulePreviewFetch(std::string mailbox_id, std::string message_id);
    void EnsureCtrlJMappingResolved();
    std::vector<std::string> SelectedMessageIds() const;
    std::size_t SelectedMessageCount() const;
    std::optional<std::string> FirstSelectedMessageId() const;
    std::string SelectedPreviewText() const;
    void ShowFindWindow();
    void ApplyFindShortcutMapping();
    bool HandleTextFind(std::string_view query, bool repeat);
    bool FindInPreview(std::string_view query, bool repeat);
    bool FindInToc(std::string_view query, bool repeat);
    MailboxFindTarget ActiveFindTarget() const;
    bool IsFocusWithin(const BView* ancestor) const;
    std::string SelectedFocusedText() const;
    bool ShouldShowJunkColumn() const;
    bool ShouldShowLabelColumn() const;
    bool ShouldShowServerStatusColumn() const;
    void UpdateTocColumnVisibility();
    void RefreshDynamicMessageMenus();
    void RefreshRecentMailboxMenu();
    void RefreshSelectedTextUrlActions();
    void RestoreFindFocus();
    void HandleGroupBySubjectToggle();
    void RefreshSearchBarRecentMenu();
    void HandleSearchBarRecentSelection(int32 index);
    void HandleSearchBarFocusChanged(bool focused);
    std::string SearchBarPromptText() const;
    std::string SearchBarQueryText() const;
    void SetSearchBarQueryText(std::string_view text, bool prompt_visible);
    void UpdateSearchBarState();
    void ExecuteSearchBarQuery();
    void HandleTocHeaderClick(BPoint where, uint32 modifiers);
    std::optional<int32> TocFieldForColumn(const class BColumn* column) const;
    void ShowMessageContextMenu(BPoint where);
    void ShowPreviewContextMenu(BPoint where);
    void ShowAttachmentContextMenu(BPoint where);
    void HandleOpenSelectedMessage();
    void HandleOpenSelectedAttachment();
    void HandleSaveSelectedAttachment();
    void HandleSaveAllAttachments();
    void HandleFetchSelectedAttachment();
    void HandleFetchSelectedMessage();
    void HandleMoveOrCopySelectedMessage(bool copy);
    void HandleCreateMailbox();
    void HandleRenameMailbox();
    void HandleDeleteMailbox();
    void HandleComposeResponse(int kind, std::string_view stationery_name = {});
    void HandleSendImmediately();
    void HandleChangeQueueingPrompt();
    void HandleChangeQueueing(int delay_seconds);
    void HandleToggleSelectedStatus();
    void HandlePreviousMessage();
    void HandleNextMessage();
    void HandleReplyShortcut(bool invert_reply_mapping);
    void HandleWorkOfflineToggle();
    void HandleMailTransferOptions(bool sending_only);
    void HandleEmptyTrash();
    void HandleTrimJunk();
    void HandleCompactMailboxes();
    void HandleForgetPasswords();
    void HandleChangePasswordDialog();
    void HandleOptionsDialog();
    void HandleMarkSelectedMessageUnread(bool unread);
    void HandleSetSelectedLegacyStatus(hermes::LegacyMessageStatus status);
    void HandleSetSelectedLabel(int label_index);
    void HandleSetSelectedPopServerStatus(hermes::PopServerStatus status);
    void HandleJunkAction(hermes::MailboxJunkAction action);
    void HandleFilterSelectedMessages();
    void HandleMakeFilter();
    void HandleSetSelectedPriority(int priority_value);
    void HandleMakeNickname();
    void HandlePrintSelectedMessage(bool preview);
    void HandleFindMessages();
    void HandleFetchSelectedMessage(bool use_default_fetch);
    void HandleRedownloadSelectedMessages(bool full);
    void HandleClearCachedSelectedMessages();
    void HandleDynamicRecipientCompose(uint32 command, std::string_view nickname_name);
    std::vector<hermes::MailboxRecord> RecentMailboxEntries(std::optional<std::string_view> account_id,
                                                            bool imap_only,
                                                            std::string_view excluded_mailbox_id);
    void HandleSelectedTextUrlCommand(int slot);
    void HandleWindowArrangement(HaikuShellHost::WindowArrangeMode mode);
    void HandleCloseAllWindows();

    HaikuShellHost& shell_host_;
    GuiPreferences gui_preferences_;
    MailboxUiSettings mailbox_ui_;
    BView* toolbar_view_ = nullptr;
    BView* search_bar_container_ = nullptr;
    BStringView* status_view_ = nullptr;
    BColumnListView* message_list_ = nullptr;
    class BColumn* status_column_ = nullptr;
    class BColumn* junk_column_ = nullptr;
    class BColumn* label_column_ = nullptr;
    class BColumn* pop_server_status_column_ = nullptr;
    BView* preview_container_ = nullptr;
    BStringView* preview_subject_ = nullptr;
    BStringView* preview_from_ = nullptr;
    BStringView* preview_to_ = nullptr;
    BStringView* preview_date_ = nullptr;
    BStringView* preview_state_ = nullptr;
    BView* preview_plain_root_ = nullptr;
    BTextView* preview_text_ = nullptr;
    HaikuWebKitMessageView* preview_web_view_ = nullptr;
    BView* attachment_container_ = nullptr;
    BListView* attachment_list_ = nullptr;
    BTabView* utility_tabs_ = nullptr;
    BView* utility_container_ = nullptr;
    BMenuItem* show_toolbar_item_ = nullptr;
    BMenuItem* show_search_bar_item_ = nullptr;
    BMenuItem* show_preview_item_ = nullptr;
    BMenuItem* show_utility_item_ = nullptr;
    BMenuItem* work_offline_item_ = nullptr;
    BMenuItem* group_by_subject_item_ = nullptr;
    BMenuItem* find_text_item_ = nullptr;
    BMenuItem* find_messages_item_ = nullptr;
    BMenuItem* find_again_item_ = nullptr;
    BMenuItem* reply_item_ = nullptr;
    BMenuItem* reply_all_item_ = nullptr;
    BMenuItem* send_immediately_item_ = nullptr;
    BMenu* edit_menu_ = nullptr;
    BMenu* search_bar_scope_menu_ = nullptr;
    BMenu* search_bar_recent_menu_ = nullptr;
    BMenu* recent_mailboxes_menu_ = nullptr;
    BMenu* new_message_to_menu_ = nullptr;
    BMenu* forward_to_menu_ = nullptr;
    BMenu* redirect_to_menu_ = nullptr;
    BMenu* new_message_with_menu_ = nullptr;
    BMenu* reply_with_menu_ = nullptr;
    BMenu* reply_all_with_menu_ = nullptr;
    BMenu* change_personality_menu_ = nullptr;
    BMenuField* search_bar_scope_field_ = nullptr;
    BMenuField* search_bar_recent_field_ = nullptr;
    BTextControl* search_bar_query_control_ = nullptr;
    BButton* search_bar_button_ = nullptr;
    std::vector<BMenuItem*> label_menu_items_;
    std::vector<BMenuItem*> selected_text_url_menu_items_;
    std::map<uint32, std::vector<BMenuItem*>> command_menu_items_;
    std::vector<SelectedTextUrlAction> selected_text_url_actions_;
    std::unique_ptr<BMessageRunner> preview_read_runner_;
    std::unique_ptr<BMessageRunner> preview_refresh_runner_;
    std::vector<std::string> mailbox_ids_;
    std::vector<std::pair<class BColumn*, int32>> toc_columns_;
    std::vector<std::size_t> attachment_indices_;
    std::vector<TaskEntryMetadata> task_entries_;
    std::vector<TaskErrorEntry> task_error_entries_;
    std::string current_mailbox_id_ = "inbox";
    std::string current_message_id_;
    SearchBarMode search_bar_mode_ = SearchBarMode::kSearchWeb;
    bool search_bar_prompt_visible_ = false;
    bool updating_search_bar_query_ = false;
    std::string preview_render_notice_;
    std::string preview_fetch_attempt_mailbox_id_;
    std::string preview_fetch_attempt_message_id_;
    std::size_t last_task_row_count_ = 0;
    std::size_t last_error_row_count_ = 0;
    std::unique_ptr<HaikuToolbarCustomizationWindow> toolbar_customization_window_;
    BWindow* find_window_ = nullptr;
    BView* last_find_focus_view_ = nullptr;
};

}  // namespace hemera::haiku
