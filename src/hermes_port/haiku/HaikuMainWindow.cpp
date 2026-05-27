#include "HaikuMainWindow.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <optional>
#include <sstream>

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <Entry.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include "HaikuShellHost.h"
#include "hermes/ComposeMessage.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"
#include "hermes/WorkspaceModel.h"

namespace hermes::haiku_port {

namespace {

constexpr uint32_t kNewComposeMessage = 'ncmp';
constexpr uint32_t kMailboxSelectedMessage = 'mbox';
constexpr uint32_t kMessageSelectedMessage = 'mmsg';
constexpr uint32_t kAttachmentSelectedMessage = 'atts';
constexpr uint32_t kTaskSelectedMessage = 'tsks';
constexpr uint32_t kSendQueuedMessage = 'sndq';
constexpr uint32_t kCheckMailMessage = 'ckml';
constexpr uint32_t kSendReceiveMessage = 'sdrx';
constexpr uint32_t kStopTasksMessage = 'stpt';
constexpr uint32_t kRefreshMailboxMessage = 'mbrf';
constexpr uint32_t kResyncMailboxMessage = 'mbrs';
constexpr uint32_t kCreateMailboxMessage = 'mbcr';
constexpr uint32_t kRenameMailboxMessage = 'mbrn';
constexpr uint32_t kDeleteMailboxMessage = 'mbdl';
constexpr uint32_t kCreateMailboxConfirmed = 'mbcc';
constexpr uint32_t kRenameMailboxConfirmed = 'mbrc';
constexpr uint32_t kDeleteMessageMessage = 'msgd';
constexpr uint32_t kUndeleteMessageMessage = 'msgu';
constexpr uint32_t kMoveMessageMessage = 'msgm';
constexpr uint32_t kCopyMessageMessage = 'msgc';
constexpr uint32_t kPerformMoveMessage = 'pmov';
constexpr uint32_t kPerformCopyMessage = 'pcpy';
constexpr uint32_t kFetchFullMessageMessage = 'msgf';
constexpr uint32_t kOpenAttachmentMessage = 'atop';
constexpr uint32_t kSaveAttachmentMessage = 'atsv';
constexpr uint32_t kSaveAllAttachmentsMessage = 'atsa';
constexpr uint32_t kFetchAttachmentMessage = 'atfe';
constexpr uint32_t kRetryTaskMessage = 'trty';
constexpr uint32_t kCancelTaskMessage = 'tcnl';
constexpr uint32_t kPromptAcceptedMessage = 'prok';
constexpr uint32_t kPromptCancelledMessage = 'prcl';

hermes::ComposeMessage BuildDefaultComposeMessage(HaikuShellHost& shell_host) {
    hermes::ComposeMessage message;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    message.id = "compose-" + std::to_string(static_cast<long long>(micros));
    message.policy = hermes::ComposePolicyFromSettings(shell_host.Settings());
    message.signature_name = message.policy.default_signature_name;
    message.stationery_name = message.policy.default_stationery_name;
    return message;
}

std::filesystem::path DefaultAttachmentSaveRoot() {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Downloads" / "HermesHemera";
    }
    return std::filesystem::temp_directory_path() / "HermesHemera";
}

std::string JoinLines(const std::vector<std::string>& lines) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index != 0) {
            stream << '\n';
        }
        stream << lines[index];
    }
    return stream.str();
}

std::string ActionKindLabel(ImapActionKind kind) {
    switch (kind) {
        case ImapActionKind::kDelete:
            return "Delete";
        case ImapActionKind::kUndelete:
            return "Undelete";
        case ImapActionKind::kMove:
            return "Move";
        case ImapActionKind::kCopy:
            return "Copy";
        case ImapActionKind::kCreateMailbox:
            return "Create Mailbox";
        case ImapActionKind::kRenameMailbox:
            return "Rename Mailbox";
        case ImapActionKind::kDeleteMailbox:
            return "Delete Mailbox";
        case ImapActionKind::kFetchAttachment:
            return "Fetch Attachment";
        case ImapActionKind::kFetchFullMessage:
            return "Fetch Full Message";
        case ImapActionKind::kResyncMailbox:
            return "Resync Mailbox";
        case ImapActionKind::kRefreshMailboxList:
            return "Refresh Mailboxes";
    }
    return "IMAP Action";
}

std::string ActionStateLabel(ImapActionState state) {
    switch (state) {
        case ImapActionState::kPending:
            return "Pending";
        case ImapActionState::kRunning:
            return "Running";
        case ImapActionState::kFailed:
            return "Failed";
        case ImapActionState::kCompleted:
            return "Completed";
        case ImapActionState::kCancelled:
            return "Cancelled";
    }
    return "Pending";
}

std::string AttachmentLabel(const AttachmentSummary& attachment) {
    std::ostringstream label;
    label << (attachment.name.empty() ? "(unnamed attachment)" : attachment.name);
    if (attachment.size > 0) {
        label << " (" << attachment.size << " bytes)";
    }
    if (attachment.omitted || !attachment.download_complete) {
        label << " [fetch required]";
    }
    if (!attachment.fetch_error.empty()) {
        label << " [error: " << attachment.fetch_error << "]";
    }
    return label.str();
}

bool LaunchPath(const std::filesystem::path& path) {
    if (be_roster == nullptr) {
        return false;
    }
    BEntry entry(path.c_str(), true);
    if (entry.InitCheck() != B_OK) {
        return false;
    }
    entry_ref ref;
    if (entry.GetRef(&ref) != B_OK) {
        return false;
    }
    return be_roster->Launch(&ref) == B_OK;
}

void ShowInfoAlert(const char* title, const std::string& message) {
    BAlert(title, message.c_str(), "OK")->Go();
}

class ContextListView final : public BListView {
public:
    using ContextHandler = std::function<void(BPoint)>;

    ContextListView(const char* name, BMessage* selection_message, ContextHandler handler)
        : BListView(name),
          handler_(std::move(handler)) {
        if (selection_message != nullptr) {
            SetSelectionMessage(selection_message);
        }
    }

    void MouseDown(BPoint where) override {
        uint32 buttons = 0;
        GetMouse(&where, &buttons, false);
        if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
            const int32 index = IndexOf(where);
            if (index >= 0) {
                Select(index);
            }
            if (handler_) {
                handler_(where);
            }
            return;
        }
        BListView::MouseDown(where);
    }

private:
    ContextHandler handler_;
};

class TextPromptWindow final : public BWindow {
public:
    TextPromptWindow(const char* title,
                     const char* label,
                     const std::string& initial_value,
                     const BMessenger& target,
                     BMessage payload)
        : BWindow(BRect(0, 0, 360, 120),
                  title,
                  B_TITLED_WINDOW,
                  B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
          target_(target),
          payload_(std::move(payload)) {
        input_ = new BTextControl("prompt-input", label, initial_value.c_str(), nullptr);

        auto* cancel = new BButton("cancel-button", "Cancel", new BMessage(kPromptCancelledMessage));
        auto* ok = new BButton("ok-button", "OK", new BMessage(kPromptAcceptedMessage));
        SetDefaultButton(ok);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(input_)
            .AddGroup(B_HORIZONTAL, 8)
                .AddGlue()
                .Add(cancel)
                .Add(ok)
            .End();
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kPromptAcceptedMessage: {
                BMessage response(payload_);
                response.AddString("name", input_->Text());
                target_.SendMessage(&response);
                PostMessage(B_QUIT_REQUESTED);
                return;
            }

            case kPromptCancelledMessage:
                PostMessage(B_QUIT_REQUESTED);
                return;

            default:
                BWindow::MessageReceived(message);
                return;
        }
    }

private:
    BMessenger target_;
    BMessage payload_;
    BTextControl* input_ = nullptr;
};

}  // namespace

HaikuMainWindow::HaikuMainWindow(HaikuShellHost& shell_host)
    : BWindow(BRect(100, 100, 1180, 860),
              "Hermes Hemera",
              B_TITLED_WINDOW,
              B_QUIT_ON_WINDOW_CLOSE),
      shell_host_(shell_host) {
    auto* menu_bar = new BMenuBar("menu-bar");

    auto* file_menu = new BMenu("File");
    file_menu->AddItem(new BMenuItem("New Message", new BMessage(kNewComposeMessage)));
    file_menu->AddSeparatorItem();
    file_menu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED)));
    menu_bar->AddItem(file_menu);

    auto* mail_menu = new BMenu("Mail");
    mail_menu->AddItem(new BMenuItem("Check Mail", new BMessage(kCheckMailMessage)));
    mail_menu->AddItem(new BMenuItem("Send Queued", new BMessage(kSendQueuedMessage)));
    mail_menu->AddItem(new BMenuItem("Send & Receive", new BMessage(kSendReceiveMessage)));
    mail_menu->AddItem(new BMenuItem("Stop Tasks", new BMessage(kStopTasksMessage)));
    menu_bar->AddItem(mail_menu);

    auto* mailbox_menu = new BMenu("Mailbox");
    mailbox_menu->AddItem(new BMenuItem("Refresh", new BMessage(kRefreshMailboxMessage)));
    mailbox_menu->AddItem(new BMenuItem("Resync", new BMessage(kResyncMailboxMessage)));
    mailbox_menu->AddSeparatorItem();
    mailbox_menu->AddItem(new BMenuItem("Create Remote Mailbox", new BMessage(kCreateMailboxMessage)));
    mailbox_menu->AddItem(new BMenuItem("Rename Remote Mailbox", new BMessage(kRenameMailboxMessage)));
    mailbox_menu->AddItem(new BMenuItem("Delete Remote Mailbox", new BMessage(kDeleteMailboxMessage)));
    menu_bar->AddItem(mailbox_menu);

    auto* message_menu = new BMenu("Message");
    message_menu->AddItem(new BMenuItem("Delete", new BMessage(kDeleteMessageMessage)));
    message_menu->AddItem(new BMenuItem("Undelete", new BMessage(kUndeleteMessageMessage)));
    message_menu->AddSeparatorItem();
    message_menu->AddItem(new BMenuItem("Move", new BMessage(kMoveMessageMessage)));
    message_menu->AddItem(new BMenuItem("Copy", new BMessage(kCopyMessageMessage)));
    message_menu->AddSeparatorItem();
    message_menu->AddItem(new BMenuItem("Fetch Full Message", new BMessage(kFetchFullMessageMessage)));
    menu_bar->AddItem(message_menu);

    auto* attachment_menu = new BMenu("Attachments");
    attachment_menu->AddItem(new BMenuItem("Open", new BMessage(kOpenAttachmentMessage)));
    attachment_menu->AddItem(new BMenuItem("Save", new BMessage(kSaveAttachmentMessage)));
    attachment_menu->AddItem(new BMenuItem("Save All", new BMessage(kSaveAllAttachmentsMessage)));
    attachment_menu->AddSeparatorItem();
    attachment_menu->AddItem(new BMenuItem("Fetch", new BMessage(kFetchAttachmentMessage)));
    menu_bar->AddItem(attachment_menu);

    auto* task_menu = new BMenu("Tasks");
    task_menu->AddItem(new BMenuItem("Retry Selected Action", new BMessage(kRetryTaskMessage)));
    task_menu->AddItem(new BMenuItem("Cancel Selected Action", new BMessage(kCancelTaskMessage)));
    menu_bar->AddItem(task_menu);

    mailbox_list_ = new ContextListView("mailboxes",
                                        new BMessage(kMailboxSelectedMessage),
                                        [this](BPoint where) { ShowMailboxContextMenu(where); });
    message_list_ = new ContextListView("messages",
                                        new BMessage(kMessageSelectedMessage),
                                        [this](BPoint where) { ShowMessageContextMenu(where); });
    attachment_list_ = new ContextListView("attachments",
                                           new BMessage(kAttachmentSelectedMessage),
                                           [this](BPoint where) { ShowAttachmentContextMenu(where); });
    task_list_ = new ContextListView("tasks",
                                     new BMessage(kTaskSelectedMessage),
                                     [this](BPoint where) { ShowTaskContextMenu(where); });

    preview_text_ = new BTextView("preview");
    preview_text_->MakeEditable(false);
    task_errors_ = new BTextView("task-errors");
    task_errors_->MakeEditable(false);

    auto* top_split = new BSplitView(B_HORIZONTAL);
    top_split->AddChild(new BScrollView("mailboxes-scroll", mailbox_list_, 0, false, true));
    top_split->AddChild(new BScrollView("messages-scroll", message_list_, 0, false, true));

    auto* preview_split = new BSplitView(B_VERTICAL);
    preview_split->AddChild(new BScrollView("preview-scroll", preview_text_, 0, true, true));
    preview_split->AddChild(new BScrollView("attachments-scroll", attachment_list_, 0, false, true));

    auto* main_split = new BSplitView(B_VERTICAL);
    main_split->AddChild(top_split);
    main_split->AddChild(preview_split);
    main_split->AddChild(new BScrollView("tasks-scroll", task_list_, 0, false, true));
    main_split->AddChild(new BScrollView("task-errors-scroll", task_errors_, 0, true, true));

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu_bar)
        .Add(main_split);

    PopulateWorkspace();
    PopulateTaskStatus();
}

HaikuMainWindow::~HaikuMainWindow() = default;

void HaikuMainWindow::MessageReceived(BMessage* message) {
    switch (message->what) {
        case kNewComposeMessage:
            shell_host_.OpenComposer(BuildDefaultComposeMessage(shell_host_));
            return;

        case kMailboxSelectedMessage:
            if (const int32 index = mailbox_list_->CurrentSelection();
                index >= 0 && static_cast<std::size_t>(index) < mailbox_ids_.size()) {
                current_mailbox_id_ = mailbox_ids_[static_cast<std::size_t>(index)];
                PopulateMessagesForCurrentMailbox();
                PopulatePreview();
            }
            return;

        case kMessageSelectedMessage:
            PopulatePreview();
            return;

        case kAttachmentSelectedMessage:
        case kTaskSelectedMessage:
            return;

        case kSendQueuedMessage:
            if (!shell_host_.SendQueued()) {
                ShowInfoAlert("send-queued-alert", "Unable to send queued mail.");
            }
            PopulateTaskStatus();
            return;

        case kCheckMailMessage:
            if (!shell_host_.CheckMail()) {
                ShowInfoAlert("check-mail-alert", "Mail check reported warnings or errors.");
            }
            PopulateTaskStatus();
            return;

        case kSendReceiveMessage:
            if (!shell_host_.SendAndReceive()) {
                ShowInfoAlert("send-receive-alert", "Send and receive reported warnings or errors.");
            }
            PopulateTaskStatus();
            return;

        case kStopTasksMessage:
            shell_host_.StopActiveTasks();
            PopulateTaskStatus();
            return;

        case kRefreshMailboxMessage:
            if (!current_mailbox_id_.empty()) {
                if (!shell_host_.RefreshMailbox(current_mailbox_id_)) {
                    ShowInfoAlert("refresh-mailbox-alert", "Unable to refresh the selected mailbox.");
                }
                PopulateTaskStatus();
            }
            return;

        case kResyncMailboxMessage:
            if (!current_mailbox_id_.empty()) {
                if (!shell_host_.ResyncMailbox(current_mailbox_id_)) {
                    ShowInfoAlert("resync-mailbox-alert", "Unable to resync the selected mailbox.");
                }
                PopulateTaskStatus();
            }
            return;

        case kCreateMailboxMessage:
            HandleCreateMailbox();
            return;

        case kRenameMailboxMessage:
            HandleRenameMailbox();
            return;

        case kDeleteMailboxMessage:
            HandleDeleteMailbox();
            return;

        case kCreateMailboxConfirmed: {
            const char* account_id = nullptr;
            const char* name = nullptr;
            if (message->FindString("account_id", &account_id) == B_OK &&
                message->FindString("name", &name) == B_OK && account_id != nullptr && name != nullptr) {
                if (!shell_host_.CreateRemoteMailbox(account_id, name)) {
                    ShowInfoAlert("create-mailbox-alert", "Unable to queue remote mailbox creation.");
                }
                PopulateTaskStatus();
            }
            return;
        }

        case kRenameMailboxConfirmed: {
            const char* mailbox_id = nullptr;
            const char* name = nullptr;
            if (message->FindString("mailbox_id", &mailbox_id) == B_OK &&
                message->FindString("name", &name) == B_OK && mailbox_id != nullptr && name != nullptr) {
                if (!shell_host_.RenameRemoteMailbox(mailbox_id, name)) {
                    ShowInfoAlert("rename-mailbox-alert", "Unable to queue remote mailbox rename.");
                }
                PopulateTaskStatus();
            }
            return;
        }

        case kDeleteMessageMessage:
            if (!current_message_id_.empty() &&
                !shell_host_.DeleteMessage(current_mailbox_id_, current_message_id_)) {
                ShowInfoAlert("delete-message-alert", "Unable to queue message deletion.");
            }
            PopulateTaskStatus();
            return;

        case kUndeleteMessageMessage:
            if (!current_message_id_.empty() &&
                !shell_host_.UndeleteMessage(current_mailbox_id_, current_message_id_)) {
                ShowInfoAlert("undelete-message-alert", "Unable to queue message undeletion.");
            }
            PopulateTaskStatus();
            return;

        case kMoveMessageMessage:
            HandleMoveOrCopySelectedMessage(false);
            return;

        case kCopyMessageMessage:
            HandleMoveOrCopySelectedMessage(true);
            return;

        case kPerformMoveMessage: {
            const char* destination_mailbox_id = nullptr;
            if (message->FindString("destination_mailbox_id", &destination_mailbox_id) == B_OK &&
                destination_mailbox_id != nullptr && !current_message_id_.empty()) {
                if (!shell_host_.MoveMessage(current_mailbox_id_, current_message_id_, destination_mailbox_id)) {
                    ShowInfoAlert("move-message-alert", "Unable to queue message move.");
                }
                PopulateTaskStatus();
            }
            return;
        }

        case kPerformCopyMessage: {
            const char* destination_mailbox_id = nullptr;
            if (message->FindString("destination_mailbox_id", &destination_mailbox_id) == B_OK &&
                destination_mailbox_id != nullptr && !current_message_id_.empty()) {
                if (!shell_host_.CopyMessage(current_mailbox_id_, current_message_id_, destination_mailbox_id)) {
                    ShowInfoAlert("copy-message-alert", "Unable to queue message copy.");
                }
                PopulateTaskStatus();
            }
            return;
        }

        case kFetchFullMessageMessage:
            HandleFetchSelectedMessage();
            return;

        case kOpenAttachmentMessage:
            HandleOpenSelectedAttachment();
            return;

        case kSaveAttachmentMessage:
            HandleSaveSelectedAttachment();
            return;

        case kSaveAllAttachmentsMessage:
            HandleSaveAllAttachments();
            return;

        case kFetchAttachmentMessage:
            HandleFetchSelectedAttachment();
            return;

        case kRetryTaskMessage: {
            const int32 index = task_list_->CurrentSelection();
            if (index >= 0 && static_cast<std::size_t>(index) < task_entries_.size()) {
                const auto& entry = task_entries_[static_cast<std::size_t>(index)];
                if (entry.is_imap_action && entry.retryable) {
                    if (!shell_host_.RetryTask(entry.id) || !shell_host_.CheckMail()) {
                        ShowInfoAlert("retry-action-alert", "Unable to retry the selected IMAP action.");
                    }
                    PopulateTaskStatus();
                }
            }
            return;
        }

        case kCancelTaskMessage: {
            const int32 index = task_list_->CurrentSelection();
            if (index >= 0 && static_cast<std::size_t>(index) < task_entries_.size()) {
                const auto& entry = task_entries_[static_cast<std::size_t>(index)];
                if (entry.is_imap_action && entry.cancelable) {
                    if (!shell_host_.CancelTask(entry.id)) {
                        ShowInfoAlert("cancel-action-alert", "Unable to cancel the selected IMAP action.");
                    }
                    PopulateTaskStatus();
                }
            }
            return;
        }

        default:
            BWindow::MessageReceived(message);
            return;
    }
}

bool HaikuMainWindow::QuitRequested() {
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}

void HaikuMainWindow::PopulateWorkspace() {
    mailbox_list_->MakeEmpty();
    message_list_->MakeEmpty();
    mailbox_ids_.clear();
    message_ids_.clear();
    attachment_indices_.clear();

    for (const auto& mailbox : shell_host_.Workspace().Mailboxes()) {
        mailbox_list_->AddItem(new BStringItem(mailbox.display_name.c_str()));
        mailbox_ids_.push_back(mailbox.id);
    }

    if (current_mailbox_id_.empty() && !mailbox_ids_.empty()) {
        current_mailbox_id_ = mailbox_ids_.front();
    }
    if (current_mailbox_id_.empty()) {
        current_mailbox_id_ = "inbox";
    }

    int32 selected_mailbox_index = -1;
    for (std::size_t index = 0; index < mailbox_ids_.size(); ++index) {
        if (mailbox_ids_[index] == current_mailbox_id_) {
            selected_mailbox_index = static_cast<int32>(index);
            break;
        }
    }
    if (selected_mailbox_index < 0 && !mailbox_ids_.empty()) {
        selected_mailbox_index = 0;
        current_mailbox_id_ = mailbox_ids_.front();
    }
    if (selected_mailbox_index >= 0) {
        mailbox_list_->Select(selected_mailbox_index);
    }

    PopulateMessagesForCurrentMailbox();
    PopulatePreview();
}

void HaikuMainWindow::PopulateMessagesForCurrentMailbox() {
    message_list_->MakeEmpty();
    message_ids_.clear();

    const auto messages = shell_host_.Workspace().MessagesForMailbox(current_mailbox_id_);
    for (const auto& message : messages) {
        std::string label = message.subject.empty() ? "(No subject)" : message.subject;
        if (message.attachment_count > 0) {
            label += " [" + std::to_string(message.attachment_count) + " attachments]";
        }
        message_list_->AddItem(new BStringItem(label.c_str()));
        message_ids_.push_back(message.id);
    }

    int32 selected_message_index = -1;
    for (std::size_t index = 0; index < message_ids_.size(); ++index) {
        if (!current_message_id_.empty() && message_ids_[index] == current_message_id_) {
            selected_message_index = static_cast<int32>(index);
            break;
        }
    }
    if (selected_message_index < 0 && !message_ids_.empty()) {
        selected_message_index = 0;
    }
    if (selected_message_index >= 0) {
        message_list_->Select(selected_message_index);
        current_message_id_ = message_ids_[static_cast<std::size_t>(selected_message_index)];
    } else {
        current_message_id_.clear();
    }
}

void HaikuMainWindow::PopulatePreview() {
    const int32 index = message_list_->CurrentSelection();
    if (index < 0 || static_cast<std::size_t>(index) >= message_ids_.size()) {
        preview_text_->SetText("Select a message or draft to preview its body and attachments.");
        PopulateAttachments(nullptr);
        return;
    }

    current_message_id_ = message_ids_[static_cast<std::size_t>(index)];
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail) {
        preview_text_->SetText("Message details unavailable.");
        PopulateAttachments(nullptr);
        return;
    }

    std::ostringstream preview;
    preview << "Mailbox: " << detail->mailbox_id << '\n'
            << "Subject: " << detail->subject << '\n'
            << "From: " << detail->sender << '\n';
    if (!detail->recipients.empty()) {
        preview << "To: " << detail->recipients << '\n';
    }
    preview << "Download: " << (detail->download_complete ? "Complete" : "Partial") << '\n';
    if (detail->attachments_omitted) {
        preview << "Attachments: Additional payloads require fetch\n";
    } else {
        preview << "Attachments: " << detail->attachments.size() << '\n';
    }
    if (detail->deleted) {
        preview << "Deleted: Yes\n";
    }
    if (detail->flagged) {
        preview << "Flagged: Yes\n";
    }
    if (detail->answered) {
        preview << "Answered: Yes\n";
    }
    if (!detail->last_error.empty()) {
        preview << "Last error: " << detail->last_error << '\n';
    }
    preview << "\n" << detail->plain_text_body;
    preview_text_->SetText(preview.str().c_str());
    PopulateAttachments(&*detail);
}

void HaikuMainWindow::PopulateAttachments(const hermes::MessageDetail* detail) {
    attachment_list_->MakeEmpty();
    attachment_indices_.clear();
    if (detail == nullptr) {
        return;
    }

    for (std::size_t index = 0; index < detail->attachments.size(); ++index) {
        attachment_list_->AddItem(new BStringItem(AttachmentLabel(detail->attachments[index]).c_str()));
        attachment_indices_.push_back(index);
    }
}

void HaikuMainWindow::PopulateTaskStatus() {
    task_list_->MakeEmpty();
    task_entries_.clear();
    task_errors_->SetText("");

    for (const auto& task : shell_host_.Tasks().Tasks()) {
        const std::string row = task.title + " | " + task.persona + " | " + task.status + " | " + task.details +
                                " | " + std::to_string(task.so_far) + "/" + std::to_string(task.total);
        task_list_->AddItem(new BStringItem(row.c_str()));
        task_entries_.push_back({task.id, false, false, false});
    }

    for (const auto& action : shell_host_.QueuedImapActions()) {
        const std::string target =
            !action.remote_mailbox.empty() ? action.remote_mailbox
                                           : (!action.mailbox_id.empty() ? action.mailbox_id : action.account_id);
        const std::string row =
            "Queued IMAP | " + ActionKindLabel(action.kind) + " | " + ActionStateLabel(action.state) + " | " + target;
        task_list_->AddItem(new BStringItem(row.c_str()));
        task_entries_.push_back({action.id,
                                 true,
                                 action.state == ImapActionState::kPending ||
                                     action.state == ImapActionState::kFailed ||
                                     action.state == ImapActionState::kCancelled,
                                 action.state == ImapActionState::kPending ||
                                     action.state == ImapActionState::kFailed});
    }

    std::vector<std::string> errors;
    for (const auto& error : shell_host_.Tasks().Errors()) {
        std::string row = error.task_id + " [" + ToString(error.kind);
        if (!error.mechanism.empty()) {
            row += "/" + error.mechanism;
        }
        row += "]: " + error.message;
        errors.push_back(std::move(row));
    }
    for (const auto& action : shell_host_.QueuedImapActions()) {
        if (!action.last_error.empty()) {
            errors.push_back(action.id + ": " + action.last_error);
        }
    }

    task_errors_->SetText(errors.empty() ? "No task errors." : JoinLines(errors).c_str());
}

void HaikuMainWindow::ShowMailboxContextMenu(BPoint where) {
    BPopUpMenu menu("mailbox-context", false, false);
    menu.AddItem(new BMenuItem("Refresh", new BMessage(kRefreshMailboxMessage)));
    menu.AddItem(new BMenuItem("Resync", new BMessage(kResyncMailboxMessage)));
    menu.AddSeparatorItem();
    menu.AddItem(new BMenuItem("Create Remote Mailbox", new BMessage(kCreateMailboxMessage)));
    menu.AddItem(new BMenuItem("Rename Remote Mailbox", new BMessage(kRenameMailboxMessage)));
    menu.AddItem(new BMenuItem("Delete Remote Mailbox", new BMessage(kDeleteMailboxMessage)));
    menu.SetTargetForItems(this);
    if (BMenuItem* item = menu.Go(mailbox_list_->ConvertToScreen(where))) {
        item->Invoke();
    }
}

void HaikuMainWindow::ShowMessageContextMenu(BPoint where) {
    BPopUpMenu menu("message-context", false, false);
    menu.AddItem(new BMenuItem("Delete", new BMessage(kDeleteMessageMessage)));
    menu.AddItem(new BMenuItem("Undelete", new BMessage(kUndeleteMessageMessage)));
    menu.AddSeparatorItem();
    menu.AddItem(new BMenuItem("Move", new BMessage(kMoveMessageMessage)));
    menu.AddItem(new BMenuItem("Copy", new BMessage(kCopyMessageMessage)));
    menu.AddSeparatorItem();
    menu.AddItem(new BMenuItem("Fetch Full Message", new BMessage(kFetchFullMessageMessage)));
    menu.SetTargetForItems(this);
    if (BMenuItem* item = menu.Go(message_list_->ConvertToScreen(where))) {
        item->Invoke();
    }
}

void HaikuMainWindow::ShowAttachmentContextMenu(BPoint where) {
    BPopUpMenu menu("attachment-context", false, false);
    menu.AddItem(new BMenuItem("Open", new BMessage(kOpenAttachmentMessage)));
    menu.AddItem(new BMenuItem("Save", new BMessage(kSaveAttachmentMessage)));
    menu.AddItem(new BMenuItem("Save All", new BMessage(kSaveAllAttachmentsMessage)));
    menu.AddSeparatorItem();
    menu.AddItem(new BMenuItem("Fetch", new BMessage(kFetchAttachmentMessage)));
    menu.SetTargetForItems(this);
    if (BMenuItem* item = menu.Go(attachment_list_->ConvertToScreen(where))) {
        item->Invoke();
    }
}

void HaikuMainWindow::ShowTaskContextMenu(BPoint where) {
    BPopUpMenu menu("task-context", false, false);
    menu.AddItem(new BMenuItem("Retry Selected Action", new BMessage(kRetryTaskMessage)));
    menu.AddItem(new BMenuItem("Cancel Selected Action", new BMessage(kCancelTaskMessage)));
    menu.AddSeparatorItem();
    menu.AddItem(new BMenuItem("Stop Tasks", new BMessage(kStopTasksMessage)));
    menu.SetTargetForItems(this);
    if (BMenuItem* item = menu.Go(task_list_->ConvertToScreen(where))) {
        item->Invoke();
    }
}

void HaikuMainWindow::HandleOpenSelectedAttachment() {
    const int32 selection = attachment_list_->CurrentSelection();
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || selection < 0 || static_cast<std::size_t>(selection) >= attachment_indices_.size()) {
        return;
    }

    const std::size_t attachment_index = attachment_indices_[static_cast<std::size_t>(selection)];
    const auto path = shell_host_.AttachmentPath(detail->mailbox_id, detail->id, attachment_index);
    if (path && std::filesystem::exists(*path) && LaunchPath(*path)) {
        return;
    }

    const auto& attachment = detail->attachments[attachment_index];
    if (attachment.omitted || !attachment.download_complete) {
        if (BAlert("attachment-fetch-alert",
                   "This attachment is not downloaded yet. Fetch it now?",
                   "Cancel",
                   "Fetch")
                ->Go() == 1) {
            HandleFetchSelectedAttachment();
        }
        return;
    }

    ShowInfoAlert("open-attachment-alert", "Unable to open the selected attachment.");
}

void HaikuMainWindow::HandleSaveSelectedAttachment() {
    const int32 selection = attachment_list_->CurrentSelection();
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || selection < 0 || static_cast<std::size_t>(selection) >= attachment_indices_.size()) {
        return;
    }

    const std::size_t attachment_index = attachment_indices_[static_cast<std::size_t>(selection)];
    const auto& attachment = detail->attachments[attachment_index];
    const std::filesystem::path destination =
        DefaultAttachmentSaveRoot() /
        (attachment.name.empty() ? ("attachment-" + std::to_string(attachment_index)) : attachment.name);

    if (!shell_host_.SaveAttachment(detail->mailbox_id, detail->id, attachment_index, destination)) {
        if (attachment.omitted || !attachment.download_complete) {
            if (BAlert("attachment-fetch-save-alert",
                       "This attachment is not downloaded yet. Fetch it now?",
                       "Cancel",
                       "Fetch")
                    ->Go() == 1) {
                HandleFetchSelectedAttachment();
            }
            return;
        }
        ShowInfoAlert("save-attachment-alert", "Unable to save the selected attachment.");
        return;
    }

    ShowInfoAlert("save-attachment-alert", "Attachment saved to:\n" + destination.string());
}

void HaikuMainWindow::HandleSaveAllAttachments() {
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || detail->attachments.empty()) {
        return;
    }

    const bool requires_fetch =
        std::any_of(detail->attachments.begin(), detail->attachments.end(), [](const AttachmentSummary& attachment) {
            return attachment.omitted || !attachment.download_complete;
        });
    if (requires_fetch) {
        if (BAlert("fetch-full-message-alert",
                   "Some attachments still need to be downloaded. Fetch the full message now?",
                   "Cancel",
                   "Fetch")
                ->Go() == 1) {
            HandleFetchSelectedMessage();
        }
        return;
    }

    const std::filesystem::path destination = DefaultAttachmentSaveRoot() / detail->id;
    if (!shell_host_.SaveAllAttachments(detail->mailbox_id, detail->id, destination)) {
        ShowInfoAlert("save-all-attachments-alert", "Unable to save all attachments.");
        return;
    }

    ShowInfoAlert("save-all-attachments-alert", "Attachments saved to:\n" + destination.string());
}

void HaikuMainWindow::HandleFetchSelectedAttachment() {
    const int32 selection = attachment_list_->CurrentSelection();
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || selection < 0 || static_cast<std::size_t>(selection) >= attachment_indices_.size()) {
        return;
    }

    const std::size_t attachment_index = attachment_indices_[static_cast<std::size_t>(selection)];
    if (!shell_host_.FetchAttachment(detail->mailbox_id, detail->id, attachment_index) || !shell_host_.CheckMail()) {
        ShowInfoAlert("fetch-attachment-alert", "Unable to fetch the selected attachment.");
    }
    PopulateTaskStatus();
}

void HaikuMainWindow::HandleFetchSelectedMessage() {
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail) {
        return;
    }
    if (!shell_host_.FetchFullMessage(detail->mailbox_id, detail->id) || !shell_host_.CheckMail()) {
        ShowInfoAlert("fetch-message-alert", "Unable to fetch the full message.");
    }
    PopulateTaskStatus();
}

void HaikuMainWindow::HandleMoveOrCopySelectedMessage(bool copy) {
    if (current_message_id_.empty()) {
        return;
    }

    const auto source_mailbox = shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
    if (!source_mailbox || source_mailbox->protocol != MailboxProtocol::kImap) {
        ShowInfoAlert(copy ? "copy-message-alert" : "move-message-alert",
                      "Message copy and move are only available for IMAP mailboxes.");
        return;
    }

    BPopUpMenu menu(copy ? "copy-message-menu" : "move-message-menu", false, false);
    for (const auto& mailbox : shell_host_.Mailboxes().ListMailboxes()) {
        if (mailbox.protocol != MailboxProtocol::kImap || mailbox.account_id != source_mailbox->account_id ||
            mailbox.id == current_mailbox_id_) {
            continue;
        }
        auto* item_message = new BMessage(copy ? kPerformCopyMessage : kPerformMoveMessage);
        item_message->AddString("destination_mailbox_id", mailbox.id.c_str());
        menu.AddItem(new BMenuItem(mailbox.display_name.c_str(), item_message));
    }
    if (menu.CountItems() == 0) {
        ShowInfoAlert(copy ? "copy-message-alert" : "move-message-alert",
                      "No destination IMAP mailboxes are available for this account.");
        return;
    }

    menu.SetTargetForItems(this);
    if (BMenuItem* item = menu.Go(ConvertToScreen(BPoint(220.0f, 220.0f)))) {
        item->Invoke();
    }
}

void HaikuMainWindow::HandleCreateMailbox() {
    std::string account_id;
    if (const auto mailbox = shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
        mailbox && mailbox->protocol == MailboxProtocol::kImap) {
        account_id = mailbox->account_id;
    } else {
        for (const auto& mailbox : shell_host_.Mailboxes().ListMailboxes()) {
            if (mailbox.protocol == MailboxProtocol::kImap) {
                account_id = mailbox.account_id;
                break;
            }
        }
    }

    if (account_id.empty()) {
        ShowInfoAlert("create-mailbox-alert", "No IMAP account is available for remote mailbox creation.");
        return;
    }

    BMessage payload(kCreateMailboxConfirmed);
    payload.AddString("account_id", account_id.c_str());
    auto* prompt =
        new TextPromptWindow("Create Remote Mailbox", "Remote mailbox name", "", BMessenger(this), payload);
    prompt->Show();
}

void HaikuMainWindow::HandleRenameMailbox() {
    const auto mailbox = shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap) {
        ShowInfoAlert("rename-mailbox-alert", "Select an IMAP mailbox to rename.");
        return;
    }

    BMessage payload(kRenameMailboxConfirmed);
    payload.AddString("mailbox_id", mailbox->id.c_str());
    auto* prompt = new TextPromptWindow("Rename Remote Mailbox",
                                        "Remote mailbox name",
                                        mailbox->remote_name.empty() ? mailbox->display_name : mailbox->remote_name,
                                        BMessenger(this),
                                        payload);
    prompt->Show();
}

void HaikuMainWindow::HandleDeleteMailbox() {
    const auto mailbox = shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap) {
        ShowInfoAlert("delete-mailbox-alert", "Select an IMAP mailbox to delete.");
        return;
    }

    if (BAlert("delete-mailbox-alert",
               ("Delete remote mailbox \"" + mailbox->display_name + "\"?").c_str(),
               "Cancel",
               "Delete")
            ->Go() != 1) {
        return;
    }

    if (!shell_host_.DeleteRemoteMailbox(mailbox->id)) {
        ShowInfoAlert("delete-mailbox-alert", "Unable to queue remote mailbox deletion.");
    }
    PopulateTaskStatus();
}

void HaikuMainWindow::RefreshWorkspace() {
    PopulateWorkspace();
    PopulateTaskStatus();
}

}  // namespace hermes::haiku_port
