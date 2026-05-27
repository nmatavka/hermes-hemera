#include "HaikuMainWindow.h"

#include <chrono>

#include <Application.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <StringItem.h>
#include <TextView.h>

#include "HaikuShellHost.h"
#include "hermes/ComposeMessage.h"
#include "hermes/WorkspaceModel.h"

namespace hermes::haiku_port {

namespace {

constexpr uint32_t kNewComposeMessage = 'ncmp';
constexpr uint32_t kMailboxSelectedMessage = 'mbox';
constexpr uint32_t kMessageSelectedMessage = 'mmsg';
constexpr uint32_t kSendQueuedMessage = 'sndq';
constexpr uint32_t kCheckMailMessage = 'ckml';
constexpr uint32_t kSendReceiveMessage = 'sdrx';
constexpr uint32_t kStopTasksMessage = 'stpt';

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

}  // namespace

HaikuMainWindow::HaikuMainWindow(HaikuShellHost& shell_host)
    : BWindow(BRect(100, 100, 1100, 800),
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

    mailbox_list_ = new BListView("mailboxes");
    mailbox_list_->SetSelectionMessage(new BMessage(kMailboxSelectedMessage));
    message_list_ = new BListView("messages");
    message_list_->SetSelectionMessage(new BMessage(kMessageSelectedMessage));
    preview_text_ = new BTextView("preview");
    preview_text_->MakeEditable(false);
    task_list_ = new BListView("tasks");
    task_errors_ = new BTextView("task-errors");
    task_errors_->MakeEditable(false);

    auto* top_split = new BSplitView(B_HORIZONTAL);
    top_split->AddChild(new BScrollView("mailboxes-scroll", mailbox_list_, 0, false, true));
    top_split->AddChild(new BScrollView("messages-scroll", message_list_, 0, false, true));

    auto* main_split = new BSplitView(B_VERTICAL);
    main_split->AddChild(top_split);
    main_split->AddChild(new BScrollView("preview-scroll", preview_text_, 0, true, true));
    main_split->AddChild(new BScrollView("tasks-scroll", task_list_, 0, false, true));
    main_split->AddChild(new BScrollView("task-errors-scroll", task_errors_, 0, true, true));

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu_bar)
        .Add(main_split);

    PopulateWorkspace();
    PopulateTaskStatus();
}

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

        case kSendQueuedMessage:
            shell_host_.SendQueued();
            PopulateTaskStatus();
            return;

        case kCheckMailMessage:
            shell_host_.CheckMail();
            PopulateTaskStatus();
            return;

        case kSendReceiveMessage:
            shell_host_.SendAndReceive();
            PopulateTaskStatus();
            return;

        case kStopTasksMessage:
            shell_host_.StopActiveTasks();
            PopulateTaskStatus();
            return;

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
        message_list_->AddItem(new BStringItem(message.subject.c_str()));
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
        preview_text_->SetText("Select a message or draft to preview its summary.");
        return;
    }

    current_message_id_ = message_ids_[static_cast<std::size_t>(index)];
    const auto summary = shell_host_.Workspace().GetMessage(current_message_id_);
    if (!summary) {
        preview_text_->SetText("Message details unavailable.");
        return;
    }

    const std::string preview =
        "Mailbox: " + summary->mailbox_id + "\nSubject: " + summary->subject + "\nFrom: " +
        summary->sender + "\nAttachments: " + std::to_string(summary->attachment_count) + "\n\n" +
        summary->preview;
    preview_text_->SetText(preview.c_str());
}

void HaikuMainWindow::RefreshWorkspace() {
    PopulateWorkspace();
    PopulateTaskStatus();
}

void HaikuMainWindow::PopulateTaskStatus() {
    task_list_->MakeEmpty();
    task_errors_->SetText("");

    for (const auto& task : shell_host_.Tasks().Tasks()) {
        const std::string row = task.title + " | " + task.persona + " | " + task.status + " | " + task.details +
                                " | " + std::to_string(task.so_far) + "/" + std::to_string(task.total);
        task_list_->AddItem(new BStringItem(row.c_str()));
    }

    std::string errors;
    for (const auto& error : shell_host_.Tasks().Errors()) {
        if (!errors.empty()) {
            errors += "\n\n";
        }
        errors += error.task_id + ": " + error.message;
    }
    if (errors.empty()) {
        errors = "No task errors.";
    }
    task_errors_->SetText(errors.c_str());
}

}  // namespace hermes::haiku_port
