#pragma once

#include <string>
#include <vector>

#include <Window.h>

class BListView;
class BTextView;

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

    void PopulateWorkspace();
    void PopulateMessagesForCurrentMailbox();
    void PopulatePreview();
    void PopulateAttachments(const hermes::MessageDetail* detail);
    void PopulateTaskStatus();
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
    BListView* mailbox_list_ = nullptr;
    BListView* message_list_ = nullptr;
    BTextView* preview_text_ = nullptr;
    BListView* attachment_list_ = nullptr;
    BListView* task_list_ = nullptr;
    BTextView* task_errors_ = nullptr;
    std::vector<std::string> mailbox_ids_;
    std::vector<std::string> message_ids_;
    std::vector<std::size_t> attachment_indices_;
    std::vector<TaskEntryMetadata> task_entries_;
    std::string current_mailbox_id_ = "inbox";
    std::string current_message_id_;
};

}  // namespace hermes::haiku_port
