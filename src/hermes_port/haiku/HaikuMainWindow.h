#pragma once

#include <memory>
#include <string>
#include <vector>

#include <OS.h>
#include <Window.h>

#include "hermes/GuiPreferences.h"

class BListView;
class BMenuItem;
class BMessageRunner;
class BStringView;
class BTabView;
class BTextView;
class BView;

namespace hermes {
struct MessageDetail;
}

namespace hermes::haiku_port {

class HaikuShellHost;

class HaikuMainWindow final : public BWindow {
public:
    explicit HaikuMainWindow(HaikuShellHost& shell_host);
    ~HaikuMainWindow() override;

    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;
    void RefreshWorkspace();

private:
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

    void PopulateWorkspace();
    void PopulateMessagesForCurrentMailbox();
    void PopulatePreview();
    void PopulateAttachments(const hermes::MessageDetail* detail);
    void PopulateTaskStatus();
    void PopulateTaskErrors();
    void PopulatePreviewHeader(const hermes::MessageDetail& detail);
    void PopulatePreviewBody(const hermes::MessageDetail& detail);
    void ApplyGuiPreferences();
    void PersistGuiPreferences();
    void UpdateViewMenuMarks();
    void TogglePreviewPane();
    void ToggleToolbar();
    void ToggleUtilityPane();
    void SelectUtilityTab(int32 index);
    void SetStatusMessage(std::string message);
    void RefreshTaskUtilityFocus(bool tasks_changed, bool errors_changed);
    void SchedulePreviewRead();
    void MarkSelectedMessageReadFromPreview();
    void CancelPreviewRead();
    void UpdateTaskErrorDetail();
    void ShowMailboxContextMenu(BPoint where);
    void ShowMessageContextMenu(BPoint where);
    void ShowAttachmentContextMenu(BPoint where);
    void ShowTaskContextMenu(BPoint where);
    void HandleOpenSelectedAttachment();
    void HandleSaveSelectedAttachment();
    void HandleSaveAllAttachments();
    void HandleFetchSelectedAttachment();
    void HandleFetchSelectedMessage();
    void HandleMoveOrCopySelectedMessage(bool copy);
    void HandleCreateMailbox();
    void HandleRenameMailbox();
    void HandleDeleteMailbox();

    HaikuShellHost& shell_host_;
    GuiPreferences gui_preferences_;
    BView* toolbar_view_ = nullptr;
    BStringView* status_view_ = nullptr;
    BListView* mailbox_list_ = nullptr;
    BListView* message_list_ = nullptr;
    BView* preview_container_ = nullptr;
    BStringView* preview_subject_ = nullptr;
    BStringView* preview_from_ = nullptr;
    BStringView* preview_to_ = nullptr;
    BStringView* preview_date_ = nullptr;
    BStringView* preview_state_ = nullptr;
    BTextView* preview_text_ = nullptr;
    BView* attachment_container_ = nullptr;
    BListView* attachment_list_ = nullptr;
    BTabView* utility_tabs_ = nullptr;
    BView* utility_container_ = nullptr;
    BListView* task_list_ = nullptr;
    BListView* task_error_list_ = nullptr;
    BTextView* task_error_detail_ = nullptr;
    BMenuItem* show_toolbar_item_ = nullptr;
    BMenuItem* show_preview_item_ = nullptr;
    BMenuItem* show_utility_item_ = nullptr;
    std::unique_ptr<BMessageRunner> preview_read_runner_;
    std::vector<std::string> mailbox_ids_;
    std::vector<std::string> message_ids_;
    std::vector<std::size_t> attachment_indices_;
    std::vector<TaskEntryMetadata> task_entries_;
    std::vector<TaskErrorEntry> task_error_entries_;
    std::string current_mailbox_id_ = "inbox";
    std::string current_message_id_;
    std::size_t last_task_row_count_ = 0;
    std::size_t last_error_row_count_ = 0;
};

}  // namespace hermes::haiku_port
