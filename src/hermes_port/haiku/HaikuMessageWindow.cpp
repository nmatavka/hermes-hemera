#include "HaikuMessageWindow.h"

#include <ctime>
#include <filesystem>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <Alert.h>
#include <Application.h>
#include <Entry.h>
#include <GroupView.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <MenuItem.h>
#include <Message.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextView.h>

#include "HaikuShellHost.h"
#include "HaikuWebKitSupport.h"
#include "hermes/MessageRenderer.h"
#include "hermes/RichTextFormat.h"
#include "hermes/WorkspaceModel.h"

namespace hemera::haiku {

namespace {

constexpr uint32_t kAttachmentSelectedMessage = 'wmat';
constexpr uint32_t kOpenAttachmentMessage = 'wato';
constexpr uint32_t kSaveAttachmentMessage = 'wats';
constexpr uint32_t kFetchAttachmentMessage = 'watf';
constexpr float kHeaderInset = 8.0f;

class ContextListView final : public BListView {
public:
    using ContextHandler = std::function<void(BPoint)>;

    ContextListView(const char* name, BMessage* selection_message, BMessage* invocation_message, ContextHandler handler)
        : BListView(name), handler_(std::move(handler)) {
        if (selection_message != nullptr) {
            SetSelectionMessage(selection_message);
        }
        if (invocation_message != nullptr) {
            SetInvocationMessage(invocation_message);
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

bool LaunchPath(const std::filesystem::path& path) {
    entry_ref ref;
    if (get_ref_for_path(path.c_str(), &ref) != B_OK) {
        return false;
    }
    return be_roster->Launch(&ref) == B_OK;
}

std::string JoinBulletList(const std::vector<std::string>& values) {
    if (values.empty()) {
        return "None";
    }
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            stream << " • ";
        }
        stream << values[index];
    }
    return stream.str();
}

std::string AttachmentLabel(const AttachmentSummary& attachment) {
    std::ostringstream stream;
    stream << (attachment.name.empty() ? "(unnamed attachment)" : attachment.name);
    if (!attachment.content_type.empty()) {
        stream << " [" << attachment.content_type << "]";
    }
    if (attachment.size > 0) {
        stream << " (" << attachment.size << " bytes)";
    }
    if (attachment.omitted || !attachment.download_complete) {
        stream << " [fetch required]";
    }
    return stream.str();
}

std::string FormatTimestamp(std::int64_t value) {
    if (value <= 0) {
        return {};
    }
    const std::time_t timestamp = static_cast<std::time_t>(value);
    char buffer[64] = {};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", std::localtime(&timestamp)) == 0) {
        return {};
    }
    return buffer;
}

}  // namespace

HaikuMessageWindow::HaikuMessageWindow(HaikuShellHost& shell_host, std::string mailbox_id, std::string message_id)
    : BWindow(BRect(180, 160, 1120, 880),
              "Message",
              B_TITLED_WINDOW,
              B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
      shell_host_(shell_host),
      mailbox_id_(std::move(mailbox_id)),
      message_id_(std::move(message_id)) {
    status_view_ = new BStringView("message-status", "Ready.");
    subject_view_ = new BStringView("message-subject", "Subject: ");
    from_view_ = new BStringView("message-from", "From: ");
    to_view_ = new BStringView("message-to", "To: ");
    date_view_ = new BStringView("message-date", "Date: ");
    state_view_ = new BStringView("message-state", "State: ");

    plain_text_ = new BTextView("message-plain");
    plain_text_->MakeEditable(false);
    plain_text_->SetWordWrap(true);
    plain_text_->SetInsets(kHeaderInset, kHeaderInset, kHeaderInset, kHeaderInset);
    plain_root_ = new BScrollView("message-plain-scroll", plain_text_, 0, true, true);

    web_view_ = new HaikuWebKitMessageView(shell_host_.DataRootPath() / "Cache" / "WebKit");
    web_view_->Hide();

    attachment_list_ = new ContextListView("message-attachments",
                                           new BMessage(kAttachmentSelectedMessage),
                                           new BMessage(kOpenAttachmentMessage),
                                           [this](BPoint where) { ShowAttachmentContextMenu(where); });
    attachment_container_ = new BGroupView(B_VERTICAL);
    BLayoutBuilder::Group<>(attachment_container_, B_VERTICAL, 6)
        .Add(new BStringView("message-attachments-heading", "Attachments"))
        .Add(new BScrollView("message-attachments-scroll", attachment_list_, 0, false, true));

    BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
        .SetInsets(B_USE_WINDOW_SPACING)
        .Add(status_view_)
        .Add(subject_view_)
        .Add(from_view_)
        .Add(to_view_)
        .Add(date_view_)
        .Add(state_view_)
        .Add(plain_root_)
        .Add(web_view_)
        .Add(attachment_container_);
}

HaikuMessageWindow::~HaikuMessageWindow() = default;

void HaikuMessageWindow::MessageReceived(BMessage* message) {
    switch (message->what) {
        case kOpenAttachmentMessage:
            HandleOpenSelectedAttachment();
            return;
        case kSaveAttachmentMessage:
            HandleSaveSelectedAttachment();
            return;
        case kFetchAttachmentMessage:
            HandleFetchSelectedAttachment();
            return;
        default:
            BWindow::MessageReceived(message);
            return;
    }
}

bool HaikuMessageWindow::QuitRequested() {
    Hide();
    return true;
}

bool HaikuMessageWindow::MatchesMessage(std::string_view mailbox_id, std::string_view message_id) const {
    return mailbox_id_ == mailbox_id && message_id_ == message_id;
}

bool HaikuMessageWindow::LoadMessage(std::string mailbox_id, std::string message_id) {
    mailbox_id_ = std::move(mailbox_id);
    message_id_ = std::move(message_id);
    const auto detail = shell_host_.WorkspaceMessageDetail(message_id_);
    if (!detail || detail->mailbox_id != mailbox_id_) {
        status_view_->SetText("Message details unavailable.");
        return false;
    }

    PopulateBody(*detail);
    PopulateHeader(*detail);
    PopulateAttachments(&*detail);
    SetTitle(detail->subject.empty() ? "Message" : detail->subject.c_str());
    return true;
}

void HaikuMessageWindow::RefreshFromWorkspace() {
    (void)LoadMessage(mailbox_id_, message_id_);
}

std::optional<hermes::MessageRenderRequest> HaikuMessageWindow::BuildRenderRequest(
    const hermes::MessageDetail& detail) const {
    hermes::MessageRenderRequest request;
    request.preferred_mode = hermes::RendererMode::kWebKit;
    request.mailbox_id = detail.mailbox_id;
    request.message_id = detail.id;
    request.html_body = detail.html_body;
    request.plain_text_body = detail.plain_text_body;
    request.rtf_body = detail.rtf_body;
    request.paige_native_body = detail.paige_native_body;
    request.styled_source = detail.styled_source;
    request.styled_fidelity = detail.styled_fidelity;
    request.allow_remote_content = true;
    request.read_only = true;
    for (std::size_t index = 0; index < detail.attachments.size(); ++index) {
        hermes::MessageRenderAttachment attachment;
        attachment.name = detail.attachments[index].name;
        attachment.content_type = detail.attachments[index].content_type;
        attachment.content_id = detail.attachments[index].content_id;
        attachment.disposition = detail.attachments[index].disposition;
        attachment.download_complete = detail.attachments[index].download_complete;
        if (const auto payload_path = shell_host_.AttachmentPath(detail.mailbox_id, detail.id, index)) {
            attachment.payload_path = *payload_path;
        }
        request.attachments.push_back(std::move(attachment));
    }
    return request;
}

void HaikuMessageWindow::PopulateHeader(const hermes::MessageDetail& detail) {
    subject_view_->SetText(("Subject: " + detail.subject).c_str());
    from_view_->SetText(("From: " + detail.sender).c_str());
    to_view_->SetText(("To: " + detail.recipients).c_str());

    std::string date_label = "Date: ";
    if (const auto record = shell_host_.Messages().GetMessage(detail.mailbox_id, detail.id)) {
        const std::string timestamp =
            !FormatTimestamp(record->updated_at).empty() ? FormatTimestamp(record->updated_at)
                                                         : FormatTimestamp(record->created_at);
        date_label += timestamp.empty() ? "Unavailable" : timestamp;
    } else if (detail.mailbox_id == "drafts") {
        date_label += "Draft";
    } else {
        date_label += "Unavailable";
    }
    date_view_->SetText(date_label.c_str());

    std::vector<std::string> state_labels;
    state_labels.push_back(detail.unread ? "Unread" : "Read");
    state_labels.push_back(detail.download_complete ? "Complete" : "Partial");
    if (detail.attachments_omitted) {
        state_labels.push_back("Attachment fetch required");
    }
    if (detail.flagged) {
        state_labels.push_back("Flagged");
    }
    if (detail.deleted) {
        state_labels.push_back("Deleted");
    }
    if (detail.answered) {
        state_labels.push_back("Answered");
    }
    if (!detail.last_error.empty()) {
        state_labels.push_back("Last error: " + detail.last_error);
    }
    if (!render_notice_.empty()) {
        state_labels.push_back(render_notice_);
    }
    state_view_->SetText(("State: " + JoinBulletList(state_labels)).c_str());
}

void HaikuMessageWindow::PopulateBody(const hermes::MessageDetail& detail) {
    const bool styled =
        !detail.html_body.empty() || !detail.rtf_body.empty() || !detail.paige_native_body.empty() ||
        detail.styled_source != hermes::StyledDocumentSource::kPlainText;
    render_notice_.clear();
    if (styled) {
        const auto request = BuildRenderRequest(detail);
        if (request && web_view_->Load(*request)) {
            plain_root_->Hide();
            web_view_->Show();
            status_view_->SetText("Styled message view active.");
            return;
        }
        render_notice_ = "Styled render unavailable; showing plain text";
    }

    web_view_->Hide();
    plain_root_->Show();
    plain_text_->SetText(detail.plain_text_body.empty() ? " " : detail.plain_text_body.c_str());
    status_view_->SetText("Message loaded.");
}

void HaikuMessageWindow::PopulateAttachments(const hermes::MessageDetail* detail) {
    attachment_list_->MakeEmpty();
    attachment_indices_.clear();
    if (detail == nullptr) {
        attachment_container_->Hide();
        return;
    }

    for (std::size_t index = 0; index < detail->attachments.size(); ++index) {
        attachment_list_->AddItem(new BStringItem(AttachmentLabel(detail->attachments[index]).c_str()));
        attachment_indices_.push_back(index);
    }
    if (detail->attachments.empty()) {
        attachment_container_->Hide();
    } else {
        attachment_container_->Show();
    }
}

void HaikuMessageWindow::ShowAttachmentContextMenu(BPoint where) {
    BPopUpMenu menu("message-attachment-context", false, false);
    menu.AddItem(new BMenuItem("Open", new BMessage(kOpenAttachmentMessage)));
    menu.AddItem(new BMenuItem("Save", new BMessage(kSaveAttachmentMessage)));
    menu.AddSeparatorItem();
    menu.AddItem(new BMenuItem("Fetch", new BMessage(kFetchAttachmentMessage)));
    menu.SetTargetForItems(this);
    BPoint screen_where = where;
    attachment_list_->ConvertToScreen(&screen_where);
    if (BMenuItem* item = menu.Go(screen_where)) {
        item->Invoke();
    }
}

void HaikuMessageWindow::HandleOpenSelectedAttachment() {
    const int32 selection = attachment_list_->CurrentSelection();
    const auto detail = shell_host_.WorkspaceMessageDetail(message_id_);
    if (!detail || selection < 0 || static_cast<std::size_t>(selection) >= attachment_indices_.size()) {
        return;
    }
    const std::size_t attachment_index = attachment_indices_[static_cast<std::size_t>(selection)];
    const auto path = shell_host_.AttachmentPath(detail->mailbox_id, detail->id, attachment_index);
    if (path && std::filesystem::exists(*path) && LaunchPath(*path)) {
        shell_host_.RecordAttachmentLaunch(detail->attachments[attachment_index].name,
                                           *path,
                                           detail->mailbox_id + ":" + detail->id);
        status_view_->SetText("Opened selected attachment.");
        return;
    }
    HandleFetchSelectedAttachment();
}

void HaikuMessageWindow::HandleSaveSelectedAttachment() {
    const int32 selection = attachment_list_->CurrentSelection();
    const auto detail = shell_host_.WorkspaceMessageDetail(message_id_);
    if (!detail || selection < 0 || static_cast<std::size_t>(selection) >= attachment_indices_.size()) {
        return;
    }
    const std::size_t attachment_index = attachment_indices_[static_cast<std::size_t>(selection)];
    std::filesystem::path destination = std::filesystem::temp_directory_path() /
                                        (detail->attachments[attachment_index].name.empty()
                                             ? "attachment"
                                             : detail->attachments[attachment_index].name);
    if (!shell_host_.SaveAttachment(detail->mailbox_id, detail->id, attachment_index, destination)) {
        status_view_->SetText("Unable to save the selected attachment.");
        return;
    }
    status_view_->SetText(("Saved attachment to " + destination.string()).c_str());
}

void HaikuMessageWindow::HandleFetchSelectedAttachment() {
    const int32 selection = attachment_list_->CurrentSelection();
    const auto detail = shell_host_.WorkspaceMessageDetail(message_id_);
    if (!detail || selection < 0 || static_cast<std::size_t>(selection) >= attachment_indices_.size()) {
        return;
    }
    const std::size_t attachment_index = attachment_indices_[static_cast<std::size_t>(selection)];
    if (!shell_host_.FetchAttachment(detail->mailbox_id, detail->id, attachment_index)) {
        status_view_->SetText("Unable to fetch the selected attachment.");
        return;
    }
    shell_host_.ReloadWorkspace();
    status_view_->SetText("Fetching selected attachment.");
}

}  // namespace hemera::haiku
