#include "HaikuMainWindow.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <Entry.h>
#include <Font.h>
#include <GroupView.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <ScrollView.h>
#include <Size.h>
#include <SplitView.h>
#include <StringItem.h>
#include <StringView.h>
#include <Tab.h>
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>
#include <View.h>

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
constexpr uint32_t kTaskErrorSelectedMessage = 'terr';
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
constexpr uint32_t kTogglePreviewPaneMessage = 'tgpp';
constexpr uint32_t kToggleToolbarMessage = 'tgtb';
constexpr uint32_t kToggleUtilityPaneMessage = 'tgup';
constexpr uint32_t kSelectTaskStatusTabMessage = 'stst';
constexpr uint32_t kSelectTaskErrorsTabMessage = 'ster';
constexpr uint32_t kOpenToolWindowMessage = 'otwl';
constexpr uint32_t kPreviewReadTickMessage = 'prrd';

constexpr float kToolbarButtonSpacing = 6.0f;
constexpr float kHeaderInset = 8.0f;
constexpr float kTaskHeaderHeight = 20.0f;
constexpr float kTaskColumnPersonaWidthDefault = 120.0f;
constexpr float kMessageItemMinHeight = 38.0f;

struct TaskColumns {
    float persona_width = kTaskColumnPersonaWidthDefault;
    float status_width = 160.0f;
    float progress_width = 106.0f;
};

ComposeMessage BuildDefaultComposeMessage(HaikuShellHost& shell_host) {
    ComposeMessage message;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    message.id = "compose-" + std::to_string(static_cast<long long>(micros));
    message.policy = ComposePolicyFromSettings(shell_host.Settings());
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

std::string JoinBulletList(const std::vector<std::string>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            stream << "  •  ";
        }
        stream << values[index];
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

std::string FormatTimestamp(std::int64_t value) {
    if (value <= 0) {
        return "";
    }

    const std::time_t timestamp = static_cast<std::time_t>(value);
    const std::tm* local_time = std::localtime(&timestamp);
    if (local_time == nullptr) {
        return "";
    }

    char buffer[64];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", local_time) == 0) {
        return "";
    }
    return buffer;
}

std::string FormatMessageSize(std::size_t size) {
    if (size >= 1024 * 1024) {
        return std::to_string(static_cast<int>(size / (1024 * 1024))) + " MB";
    }
    if (size >= 1024) {
        return std::to_string(static_cast<int>(size / 1024)) + " KB";
    }
    return std::to_string(size) + " B";
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

void ConfigureToolbarButton(BButton* button,
                            const char* tool_tip,
                            bool tool_tips_enabled) {
    if (button == nullptr) {
        return;
    }
    if (tool_tips_enabled && tool_tip != nullptr && tool_tip[0] != '\0') {
        button->SetToolTip(tool_tip);
    }
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

class MessageListItem final : public BListItem {
public:
    explicit MessageListItem(MessageSummary summary)
        : summary_(std::move(summary)) {}

    void Update(BView* owner, const BFont* font) override {
        BListItem::Update(owner, font);
        font_height height {};
        font->GetHeight(&height);
        const float line_height = std::ceil(height.ascent + height.descent + height.leading);
        SetHeight(std::max(kMessageItemMinHeight, line_height * 2.0f + 12.0f));
    }

    void DrawItem(BView* owner, BRect frame, bool complete) override {
        const rgb_color background =
            IsSelected() ? ui_color(B_LIST_SELECTED_BACKGROUND_COLOR) : ui_color(B_LIST_BACKGROUND_COLOR);
        const rgb_color text_color =
            IsSelected() ? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR) : ui_color(B_LIST_ITEM_TEXT_COLOR);
        if (complete || IsSelected()) {
            owner->SetLowColor(background);
            owner->FillRect(frame, B_SOLID_LOW);
        }

        const float left = frame.left + 8.0f;
        const float right = frame.right - 8.0f;

        BFont bold(*be_bold_font);
        BFont plain(*be_plain_font);
        font_height bold_height {};
        font_height plain_height {};
        bold.GetHeight(&bold_height);
        plain.GetHeight(&plain_height);
        const float subject_baseline = frame.top + 6.0f + std::ceil(bold_height.ascent);
        const float preview_baseline =
            subject_baseline + std::ceil(bold_height.descent + bold_height.leading + plain_height.ascent + 5.0f);

        owner->SetHighColor(text_color);
        owner->SetFont(&bold);
        std::string subject = summary_.subject.empty() ? "(No subject)" : summary_.subject;
        if (summary_.unread) {
            subject = "• " + subject;
        }
        owner->MovePenTo(BPoint(left, subject_baseline));
        owner->DrawString(subject.c_str());

        const std::string date_label = FormatTimestamp(summary_.timestamp);
        if (!date_label.empty()) {
            owner->SetFont(&plain);
            const float date_width = owner->StringWidth(date_label.c_str());
            owner->MovePenTo(BPoint(std::max(left, right - date_width), subject_baseline));
            owner->DrawString(date_label.c_str());
        }

        std::vector<std::string> meta_parts;
        if (!summary_.status.empty() && summary_.status != "received") {
            meta_parts.push_back(summary_.status);
        }
        if (!summary_.priority.empty() && summary_.priority != "normal") {
            meta_parts.push_back(summary_.priority);
        }
        if (summary_.attachment_count > 0) {
            meta_parts.push_back(std::to_string(summary_.attachment_count) +
                                 (summary_.attachment_count == 1 ? " attachment" : " attachments"));
        }
        if (summary_.attachments_omitted || !summary_.download_complete) {
            meta_parts.push_back("partial");
        }
        if (summary_.size > 0) {
            meta_parts.push_back(FormatMessageSize(summary_.size));
        }
        const std::string meta = JoinBulletList(meta_parts);

        owner->SetFont(&plain);
        owner->MovePenTo(BPoint(left, preview_baseline));
        owner->DrawString(summary_.sender.empty() ? "(unknown sender)" : summary_.sender.c_str());
        if (!meta.empty()) {
            const float meta_width = owner->StringWidth(meta.c_str());
            owner->MovePenTo(BPoint(std::max(left, right - meta_width), preview_baseline));
            owner->DrawString(meta.c_str());
        }
    }

private:
    MessageSummary summary_;
};

class MailboxListItem final : public BStringItem {
public:
    MailboxListItem(std::string label, int32 outline_level, bool emphasized)
        : BStringItem(label.c_str(), outline_level, false),
          outline_level_(outline_level),
          emphasized_(emphasized) {}

    void DrawItem(BView* owner, BRect frame, bool complete) override {
        const rgb_color background =
            IsSelected() ? ui_color(B_LIST_SELECTED_BACKGROUND_COLOR) : ui_color(B_LIST_BACKGROUND_COLOR);
        const rgb_color text_color =
            IsSelected() ? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR) : ui_color(B_LIST_ITEM_TEXT_COLOR);
        if (complete || IsSelected()) {
            owner->SetLowColor(background);
            owner->FillRect(frame, B_SOLID_LOW);
        }

        BFont font(emphasized_ ? *be_bold_font : *be_plain_font);
        font_height metrics {};
        font.GetHeight(&metrics);
        const float baseline = frame.top + 4.0f + std::ceil(metrics.ascent);
        const float left = frame.left + 8.0f + outline_level_ * 14.0f;

        owner->SetHighColor(text_color);
        owner->SetFont(&font);
        owner->MovePenTo(BPoint(left, baseline));
        owner->DrawString(Text());
        owner->SetFont(be_plain_font);
    }

private:
    int32 outline_level_ = 0;
    bool emphasized_ = false;
};

class TaskHeaderView final : public BView {
public:
    explicit TaskHeaderView(TaskColumns columns)
        : BView("task-header", B_WILL_DRAW),
          columns_(columns) {
        SetExplicitMinSize(BSize(B_SIZE_UNSET, kTaskHeaderHeight));
        SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    }

    void Draw(BRect update_rect) override {
        BView::Draw(update_rect);
        SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT));
        StrokeLine(BPoint(Bounds().left, Bounds().bottom), BPoint(Bounds().right, Bounds().bottom));

        SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
        const float left = 8.0f;
        const float persona_left = Bounds().right - (columns_.persona_width + columns_.status_width +
                                                     columns_.progress_width + 24.0f);
        const float status_left = Bounds().right - (columns_.status_width + columns_.progress_width + 16.0f);
        const float progress_left = Bounds().right - (columns_.progress_width + 8.0f);
        const float baseline = Bounds().top + 13.0f;
        DrawString("Task", BPoint(left, baseline));
        DrawString("Persona", BPoint(persona_left, baseline));
        DrawString("Status", BPoint(status_left, baseline));
        DrawString("Progress", BPoint(progress_left, baseline));
    }

private:
    TaskColumns columns_;
};

class TaskListItem final : public BListItem {
public:
    TaskListItem(std::string task,
                 std::string persona,
                 std::string status,
                 std::string details,
                 std::string progress,
                 TaskColumns columns)
        : task_(std::move(task)),
          persona_(std::move(persona)),
          status_(std::move(status)),
          details_(std::move(details)),
          progress_(std::move(progress)),
          columns_(columns) {}

    void Update(BView* owner, const BFont* font) override {
        BListItem::Update(owner, font);
        font_height height {};
        font->GetHeight(&height);
        SetHeight(std::max(22.0f, std::ceil(height.ascent + height.descent + height.leading) + 8.0f));
    }

    void DrawItem(BView* owner, BRect frame, bool complete) override {
        const rgb_color background =
            IsSelected() ? ui_color(B_LIST_SELECTED_BACKGROUND_COLOR) : ui_color(B_LIST_BACKGROUND_COLOR);
        const rgb_color text_color =
            IsSelected() ? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR) : ui_color(B_LIST_ITEM_TEXT_COLOR);
        if (complete || IsSelected()) {
            owner->SetLowColor(background);
            owner->FillRect(frame, B_SOLID_LOW);
        }

        owner->SetHighColor(text_color);
        owner->SetFont(be_plain_font);
        const float baseline = frame.top + 14.0f;
        const float task_left = frame.left + 8.0f;
        const float persona_left = frame.right - (columns_.persona_width + columns_.status_width +
                                                  columns_.progress_width + 24.0f);
        const float status_left = frame.right - (columns_.status_width + columns_.progress_width + 16.0f);
        const float progress_left = frame.right - (columns_.progress_width + 8.0f);
        DrawClippedString(owner, task_, BPoint(task_left, baseline), persona_left - task_left - 8.0f);
        DrawClippedString(owner, persona_, BPoint(persona_left, baseline), columns_.persona_width - 8.0f);
        DrawClippedString(owner, status_, BPoint(status_left, baseline), columns_.status_width - 8.0f);
        DrawClippedString(owner, progress_, BPoint(progress_left, baseline), columns_.progress_width - 4.0f);

        if (!details_.empty()) {
            rgb_color detail_color = text_color;
            detail_color.alpha = 200;
            owner->SetHighColor(detail_color);
            DrawClippedString(owner,
                              details_,
                              BPoint(status_left - owner->StringWidth(details_.c_str()) - 12.0f, baseline),
                              status_left - persona_left - 18.0f);
        }
    }

private:
    static void DrawClippedString(BView* owner,
                                  const std::string& value,
                                  BPoint point,
                                  float width) {
        if (width <= 0.0f) {
            return;
        }
        std::string text = value;
        const float ellipsis_width = owner->StringWidth("...");
        while (!text.empty() && owner->StringWidth(text.c_str()) > width) {
            text.pop_back();
        }
        if (text != value && width > ellipsis_width) {
            while (!text.empty() && owner->StringWidth((text + "...").c_str()) > width) {
                text.pop_back();
            }
            text += "...";
        }
        owner->MovePenTo(point);
        owner->DrawString(text.c_str());
    }

    std::string task_;
    std::string persona_;
    std::string status_;
    std::string details_;
    std::string progress_;
    TaskColumns columns_;
};

}  // namespace

HaikuMainWindow::HaikuMainWindow(HaikuShellHost& shell_host)
    : BWindow(BRect(100, 100, 1260, 900),
              "Hermes Hemera",
              B_TITLED_WINDOW,
              B_QUIT_ON_WINDOW_CLOSE | B_AUTO_UPDATE_SIZE_LIMITS),
      shell_host_(shell_host),
      gui_preferences_(GuiPreferencesFromSettings(shell_host.Settings())) {
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

    auto* view_menu = new BMenu("View");
    show_toolbar_item_ = new BMenuItem("Show Toolbar", new BMessage(kToggleToolbarMessage));
    show_preview_item_ = new BMenuItem("Show Preview Pane", new BMessage(kTogglePreviewPaneMessage));
    show_utility_item_ = new BMenuItem("Show Task Tools", new BMessage(kToggleUtilityPaneMessage));
    view_menu->AddItem(show_toolbar_item_);
    view_menu->AddItem(show_preview_item_);
    view_menu->AddItem(show_utility_item_);
    view_menu->AddSeparatorItem();
    view_menu->AddItem(new BMenuItem("Task Status", new BMessage(kSelectTaskStatusTabMessage)));
    view_menu->AddItem(new BMenuItem("Task Errors", new BMessage(kSelectTaskErrorsTabMessage)));
    menu_bar->AddItem(view_menu);

    auto* tools_menu = new BMenu("Tools");
    const std::vector<std::pair<const char*, const char*>> tool_entries = {
        {"Mailboxes", "mailboxes"},
        {"Task Status", "task-status"},
        {"Task Errors", "task-errors"},
        {"Signatures", "signatures"},
        {"Stationery", "stationery"},
        {"Nicknames", "nicknames"},
        {"Personalities", "personalities"},
        {"Filters", "filters"},
        {"Filter Report", "filter-report"},
        {"Directory Services", "directory-services"},
        {"File Browser", "file-browser"},
        {"Link History", "link-history"},
    };
    for (const auto& tool_entry : tool_entries) {
        auto* item_message = new BMessage(kOpenToolWindowMessage);
        item_message->AddString("tool_id", tool_entry.second);
        tools_menu->AddItem(new BMenuItem(tool_entry.first, item_message));
    }
    menu_bar->AddItem(tools_menu);

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

    toolbar_view_ = new BGroupView(B_HORIZONTAL, kToolbarButtonSpacing);
    auto* new_button = new BButton("toolbar-new", "New", new BMessage(kNewComposeMessage));
    auto* check_button = new BButton("toolbar-check", "Check Mail", new BMessage(kCheckMailMessage));
    auto* send_button = new BButton("toolbar-send", "Send Queued", new BMessage(kSendQueuedMessage));
    auto* send_receive_button =
        new BButton("toolbar-send-receive", "Send & Receive", new BMessage(kSendReceiveMessage));
    auto* stop_button = new BButton("toolbar-stop", "Stop Tasks", new BMessage(kStopTasksMessage));
    ConfigureToolbarButton(new_button, "Compose a new message", gui_preferences_.show_toolbar_tips);
    ConfigureToolbarButton(check_button, "Check all enabled accounts", gui_preferences_.show_toolbar_tips);
    ConfigureToolbarButton(send_button, "Send queued outgoing messages", gui_preferences_.show_toolbar_tips);
    ConfigureToolbarButton(
        send_receive_button, "Send queued mail and check for new mail", gui_preferences_.show_toolbar_tips);
    ConfigureToolbarButton(stop_button, "Stop active background tasks", gui_preferences_.show_toolbar_tips);
    BLayoutBuilder::Group<>(toolbar_view_, B_HORIZONTAL, kToolbarButtonSpacing)
        .SetInsets(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING, B_USE_SMALL_SPACING, 0.0f)
        .Add(new_button)
        .Add(check_button)
        .Add(send_button)
        .Add(send_receive_button)
        .Add(stop_button)
        .AddGlue();

    status_view_ = new BStringView("main-status", "Ready.");

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
    task_error_list_ = new BListView("task-errors");
    task_error_list_->SetSelectionMessage(new BMessage(kTaskErrorSelectedMessage));

    preview_subject_ = new BStringView("preview-subject", "Subject: ");
    preview_from_ = new BStringView("preview-from", "From: ");
    preview_to_ = new BStringView("preview-to", "To: ");
    preview_date_ = new BStringView("preview-date", "Date: ");
    preview_state_ = new BStringView("preview-state", "State: ");

    preview_text_ = new BTextView("preview");
    preview_text_->MakeEditable(false);
    preview_text_->SetWordWrap(true);
    preview_text_->SetInsets(kHeaderInset, kHeaderInset, kHeaderInset, kHeaderInset);

    preview_container_ = new BGroupView(B_VERTICAL);
    auto* preview_body_scroll = new BScrollView("preview-scroll", preview_text_, 0, true, true);

    attachment_container_ = new BGroupView(B_VERTICAL);
    auto* attachment_heading = new BStringView("attachment-heading", "Attachments");
    auto* attachment_scroll =
        new BScrollView("attachments-scroll", attachment_list_, 0, false, true);
    BLayoutBuilder::Group<>(attachment_container_, B_VERTICAL, 6)
        .SetInsets(0, 0, 0, 0)
        .Add(attachment_heading)
        .Add(attachment_scroll);

    BLayoutBuilder::Group<>(preview_container_, B_VERTICAL, 6)
        .SetInsets(B_USE_SMALL_SPACING)
        .Add(preview_subject_)
        .Add(preview_from_)
        .Add(preview_to_)
        .Add(preview_date_)
        .Add(preview_state_)
        .Add(preview_body_scroll)
        .Add(attachment_container_);

    TaskColumns task_columns;
    task_columns.persona_width = static_cast<float>(gui_preferences_.task_status_state_width);
    task_columns.status_width = static_cast<float>(gui_preferences_.task_status_status_width);
    task_columns.progress_width = static_cast<float>(gui_preferences_.task_status_progress_width);

    utility_tabs_ = new BTabView("main-utility-tabs");
    utility_container_ = new BGroupView(B_VERTICAL);
    utility_container_->SetExplicitMinSize(BSize(B_SIZE_UNSET, gui_preferences_.utility_pane_height));

    auto* task_status_tab = new BGroupView(B_VERTICAL);
    auto* task_header = new TaskHeaderView(task_columns);
    auto* task_scroll = new BScrollView("tasks-scroll", task_list_, 0, false, true);
    BLayoutBuilder::Group<>(task_status_tab, B_VERTICAL, 0)
        .Add(task_header)
        .Add(task_scroll);
    utility_tabs_->AddTab(task_status_tab);
    if (BTab* tab = utility_tabs_->TabAt(0)) {
        tab->SetLabel("Task Status");
    }

    task_error_detail_ = new BTextView("task-error-detail");
    task_error_detail_->MakeEditable(false);
    task_error_detail_->SetWordWrap(true);
    task_error_detail_->SetInsets(kHeaderInset, kHeaderInset, kHeaderInset, kHeaderInset);
    auto* task_error_detail_scroll =
        new BScrollView("task-error-detail-scroll", task_error_detail_, 0, true, true);
    auto* task_error_list_scroll =
        new BScrollView("task-error-list-scroll", task_error_list_, 0, false, true);
    auto* task_error_split = new BSplitView(B_VERTICAL);
    task_error_split->AddChild(task_error_list_scroll);
    task_error_split->AddChild(task_error_detail_scroll);
    utility_tabs_->AddTab(task_error_split);
    if (BTab* tab = utility_tabs_->TabAt(1)) {
        tab->SetLabel("Task Errors");
    }

    BLayoutBuilder::Group<>(utility_container_, B_VERTICAL, 0)
        .Add(utility_tabs_);

    auto* top_split = new BSplitView(B_HORIZONTAL);
    top_split->AddChild(new BScrollView("mailboxes-scroll", mailbox_list_, 0, false, true));
    auto* right_split = new BSplitView(B_VERTICAL);
    right_split->AddChild(new BScrollView("messages-scroll", message_list_, 0, false, true));
    right_split->AddChild(preview_container_);
    top_split->AddChild(right_split);

    auto* content_split = new BSplitView(B_VERTICAL);
    content_split->AddChild(top_split);
    content_split->AddChild(utility_container_);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu_bar)
        .Add(toolbar_view_)
        .AddGroup(B_VERTICAL, 4)
            .SetInsets(B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING, B_USE_WINDOW_SPACING, B_USE_WINDOW_SPACING)
            .Add(status_view_)
            .Add(content_split)
        .End();

    AddShortcut(B_F7_KEY, B_NO_COMMAND_KEY, new BMessage(kTogglePreviewPaneMessage));
    PopulateWorkspace();
    PopulateTaskStatus();
    if (mailbox_list_ != nullptr && mailbox_list_->Parent() != nullptr) {
        mailbox_list_->Parent()->Hide();
    }
    ApplyGuiPreferences();
}

HaikuMainWindow::~HaikuMainWindow() = default;

void HaikuMainWindow::MessageReceived(BMessage* message) {
    switch (message->what) {
        case kNewComposeMessage:
            shell_host_.OpenComposer(BuildDefaultComposeMessage(shell_host_));
            SetStatusMessage("Opened a new compose window.");
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

        case kTaskErrorSelectedMessage:
            UpdateTaskErrorDetail();
            return;

        case kSendQueuedMessage: {
            const bool success = shell_host_.SendQueued();
            PopulateTaskStatus();
            SetStatusMessage(success ? "Sent queued mail." : "Send queued reported warnings or errors.");
            return;
        }

        case kCheckMailMessage: {
            const bool success = shell_host_.CheckMail();
            PopulateTaskStatus();
            SetStatusMessage(success ? "Checked mail." : "Mail check reported warnings or errors.");
            return;
        }

        case kSendReceiveMessage: {
            const bool success = shell_host_.SendAndReceive();
            PopulateTaskStatus();
            SetStatusMessage(success ? "Send and receive complete." :
                                       "Send and receive reported warnings or errors.");
            return;
        }

        case kStopTasksMessage:
            shell_host_.StopActiveTasks();
            PopulateTaskStatus();
            SetStatusMessage("Stopped active background tasks.");
            return;

        case kRefreshMailboxMessage:
            if (!current_mailbox_id_.empty()) {
                const bool success = shell_host_.RefreshMailbox(current_mailbox_id_);
                PopulateTaskStatus();
                SetStatusMessage(success ? "Mailbox refresh queued." :
                                           "Mailbox refresh reported warnings or errors.");
            }
            return;

        case kResyncMailboxMessage:
            if (!current_mailbox_id_.empty()) {
                const bool success = shell_host_.ResyncMailbox(current_mailbox_id_);
                PopulateTaskStatus();
                SetStatusMessage(success ? "Mailbox resync queued." :
                                           "Mailbox resync reported warnings or errors.");
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
                const bool queued = shell_host_.CreateRemoteMailbox(account_id, name);
                PopulateTaskStatus();
                SetStatusMessage(
                    queued ? "Remote mailbox creation queued." : "Unable to queue remote mailbox creation.");
                if (!queued) {
                    SelectUtilityTab(1);
                }
            }
            return;
        }

        case kRenameMailboxConfirmed: {
            const char* mailbox_id = nullptr;
            const char* name = nullptr;
            if (message->FindString("mailbox_id", &mailbox_id) == B_OK &&
                message->FindString("name", &name) == B_OK && mailbox_id != nullptr && name != nullptr) {
                const bool queued = shell_host_.RenameRemoteMailbox(mailbox_id, name);
                PopulateTaskStatus();
                SetStatusMessage(
                    queued ? "Remote mailbox rename queued." : "Unable to queue remote mailbox rename.");
                if (!queued) {
                    SelectUtilityTab(1);
                }
            }
            return;
        }

        case kDeleteMessageMessage: {
            const bool queued =
                !current_message_id_.empty() &&
                shell_host_.DeleteMessage(current_mailbox_id_, current_message_id_);
            PopulateTaskStatus();
            SetStatusMessage(queued ? "Message delete queued." : "Unable to queue message deletion.");
            return;
        }

        case kUndeleteMessageMessage: {
            const bool queued =
                !current_message_id_.empty() &&
                shell_host_.UndeleteMessage(current_mailbox_id_, current_message_id_);
            PopulateTaskStatus();
            SetStatusMessage(queued ? "Message undeletion queued." :
                                      "Unable to queue message undeletion.");
            return;
        }

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
                const bool queued =
                    shell_host_.MoveMessage(current_mailbox_id_, current_message_id_, destination_mailbox_id);
                PopulateTaskStatus();
                SetStatusMessage(queued ? "Message move queued." : "Unable to queue message move.");
            }
            return;
        }

        case kPerformCopyMessage: {
            const char* destination_mailbox_id = nullptr;
            if (message->FindString("destination_mailbox_id", &destination_mailbox_id) == B_OK &&
                destination_mailbox_id != nullptr && !current_message_id_.empty()) {
                const bool queued =
                    shell_host_.CopyMessage(current_mailbox_id_, current_message_id_, destination_mailbox_id);
                PopulateTaskStatus();
                SetStatusMessage(queued ? "Message copy queued." : "Unable to queue message copy.");
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
                    const bool queued = shell_host_.RetryTask(entry.id) && shell_host_.CheckMail();
                    PopulateTaskStatus();
                    SetStatusMessage(
                        queued ? "Retry queued for selected IMAP action." :
                                 "Unable to retry the selected IMAP action.");
                }
            }
            return;
        }

        case kCancelTaskMessage: {
            const int32 index = task_list_->CurrentSelection();
            if (index >= 0 && static_cast<std::size_t>(index) < task_entries_.size()) {
                const auto& entry = task_entries_[static_cast<std::size_t>(index)];
                if (entry.is_imap_action && entry.cancelable) {
                    const bool cancelled = shell_host_.CancelTask(entry.id);
                    PopulateTaskStatus();
                    SetStatusMessage(cancelled ? "Cancelled selected IMAP action." :
                                                "Unable to cancel the selected IMAP action.");
                }
            }
            return;
        }

        case kTogglePreviewPaneMessage:
            TogglePreviewPane();
            return;

        case kToggleToolbarMessage:
            ToggleToolbar();
            return;

        case kToggleUtilityPaneMessage:
            ToggleUtilityPane();
            return;

        case kSelectTaskStatusTabMessage:
            SelectUtilityTab(0);
            return;

        case kSelectTaskErrorsTabMessage:
            SelectUtilityTab(1);
            return;

        case kOpenToolWindowMessage: {
            const char* tool_id = nullptr;
            if (message->FindString("tool_id", &tool_id) == B_OK && tool_id != nullptr) {
                shell_host_.OpenToolWindow(tool_id);
            }
            return;
        }

        case kPreviewReadTickMessage:
            MarkSelectedMessageReadFromPreview();
            return;

        default:
            BWindow::MessageReceived(message);
            return;
    }
}

bool HaikuMainWindow::QuitRequested() {
    PersistGuiPreferences();
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}

void HaikuMainWindow::PopulateWorkspace() {
    mailbox_list_->MakeEmpty();
    message_list_->MakeEmpty();
    mailbox_ids_.clear();
    message_ids_.clear();
    attachment_indices_.clear();

    auto mailboxes = shell_host_.Workspace().Mailboxes();
    std::stable_sort(mailboxes.begin(), mailboxes.end(), [](const MailboxSummary& left, const MailboxSummary& right) {
        if (left.account_id != right.account_id) {
            return left.account_id < right.account_id;
        }
        if (left.system_mailbox != right.system_mailbox) {
            return left.system_mailbox && !right.system_mailbox;
        }
        return left.display_name < right.display_name;
    });

    std::map<std::string, std::string> parent_by_id;
    for (const auto& mailbox : mailboxes) {
        parent_by_id.emplace(mailbox.id, mailbox.parent_id);
    }

    const auto depth_for = [&parent_by_id](std::string mailbox_id) {
        int32 depth = 0;
        auto current = parent_by_id.find(mailbox_id);
        while (current != parent_by_id.end() && !current->second.empty()) {
            ++depth;
            if (current->second.rfind("account:", 0) == 0) {
                break;
            }
            current = parent_by_id.find(current->second);
        }
        return depth;
    };

    for (const auto& mailbox : mailboxes) {
        std::string label = mailbox.display_name;
        if (mailbox.unread_count > 0) {
            label += " (" + std::to_string(mailbox.unread_count) + ")";
        }
        if (mailbox.protocol == "imap" && mailbox.is_remote && !mailbox.parent_id.empty() &&
            mailbox.parent_id.rfind("account:", 0) != 0) {
            const std::size_t split = label.find_last_of("/.");
            if (split != std::string::npos && split + 1 < label.size()) {
                label = label.substr(split + 1);
            }
        }
        mailbox_list_->AddItem(
            new MailboxListItem(label, depth_for(mailbox.id), mailbox.system_mailbox || mailbox.parent_id.empty()));
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

    auto messages = shell_host_.Workspace().MessagesForMailbox(current_mailbox_id_);
    std::stable_sort(messages.begin(), messages.end(), [](const MessageSummary& left, const MessageSummary& right) {
        if (left.unread != right.unread) {
            return left.unread && !right.unread;
        }
        if (left.timestamp != right.timestamp) {
            return left.timestamp > right.timestamp;
        }
        return left.subject < right.subject;
    });
    for (const auto& message : messages) {
        message_list_->AddItem(new MessageListItem(message));
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

void HaikuMainWindow::PopulatePreviewHeader(const hermes::MessageDetail& detail) {
    preview_subject_->SetText(("Subject: " + detail.subject).c_str());
    preview_from_->SetText(("From: " + detail.sender).c_str());
    preview_to_->SetText(("To: " + detail.recipients).c_str());

    std::string date_label = "Date: ";
    if (const auto record = shell_host_.Messages().GetMessage(detail.mailbox_id, detail.id)) {
        const std::string timestamp =
            !FormatTimestamp(record->updated_at).empty() ? FormatTimestamp(record->updated_at)
                                                         : FormatTimestamp(record->created_at);
        if (!timestamp.empty()) {
            date_label += timestamp;
        } else {
            date_label += "Unavailable";
        }
    } else if (detail.mailbox_id == "drafts") {
        date_label += "Draft";
    } else {
        date_label += "Unavailable";
    }
    preview_date_->SetText(date_label.c_str());

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
    preview_state_->SetText(("State: " + JoinBulletList(state_labels)).c_str());
}

void HaikuMainWindow::PopulatePreviewBody(const hermes::MessageDetail& detail) {
    preview_text_->SetText(detail.plain_text_body.empty() ? " " : detail.plain_text_body.c_str());
}

void HaikuMainWindow::PopulatePreview() {
    const int32 index = message_list_->CurrentSelection();
    if (index < 0 || static_cast<std::size_t>(index) >= message_ids_.size()) {
        preview_subject_->SetText("Subject: ");
        preview_from_->SetText("From: ");
        preview_to_->SetText("To: ");
        preview_date_->SetText("Date: ");
        preview_state_->SetText("State: ");
        preview_text_->SetText("Select a message or draft to preview it.");
        PopulateAttachments(nullptr);
        CancelPreviewRead();
        return;
    }

    current_message_id_ = message_ids_[static_cast<std::size_t>(index)];
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail) {
        preview_subject_->SetText("Subject: ");
        preview_from_->SetText("From: ");
        preview_to_->SetText("To: ");
        preview_date_->SetText("Date: ");
        preview_state_->SetText("State: ");
        preview_text_->SetText("Message details unavailable.");
        PopulateAttachments(nullptr);
        CancelPreviewRead();
        return;
    }

    PopulatePreviewHeader(*detail);
    PopulatePreviewBody(*detail);
    PopulateAttachments(&*detail);
    SchedulePreviewRead();
}

void HaikuMainWindow::PopulateAttachments(const hermes::MessageDetail* detail) {
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

void HaikuMainWindow::PopulateTaskStatus() {
    task_list_->MakeEmpty();
    task_entries_.clear();

    TaskColumns task_columns;
    task_columns.persona_width = static_cast<float>(gui_preferences_.task_status_state_width);
    task_columns.status_width = static_cast<float>(gui_preferences_.task_status_status_width);
    task_columns.progress_width = static_cast<float>(gui_preferences_.task_status_progress_width);

    for (const auto& task : shell_host_.Tasks().Tasks()) {
        std::ostringstream progress;
        if (task.total > 0) {
            progress << task.so_far << "/" << task.total;
        } else if (task.so_far > 0) {
            progress << task.so_far;
        }
        task_list_->AddItem(new TaskListItem(task.title,
                                             task.persona.empty() ? "Default" : task.persona,
                                             task.status,
                                             task.details,
                                             progress.str(),
                                             task_columns));
        task_entries_.push_back({task.id, false, false, false});
    }

    for (const auto& action : shell_host_.QueuedImapActions()) {
        const std::string target =
            !action.remote_mailbox.empty() ? action.remote_mailbox
                                           : (!action.mailbox_id.empty() ? action.mailbox_id : action.account_id);
        task_list_->AddItem(new TaskListItem("Queued IMAP",
                                             action.account_id,
                                             ActionStateLabel(action.state),
                                             ActionKindLabel(action.kind) + (target.empty() ? "" : " • " + target),
                                             "",
                                             task_columns));
        task_entries_.push_back({action.id,
                                 true,
                                 action.state == ImapActionState::kPending ||
                                     action.state == ImapActionState::kFailed ||
                                     action.state == ImapActionState::kCancelled,
                                 action.state == ImapActionState::kPending ||
                                     action.state == ImapActionState::kFailed});
    }

    PopulateTaskErrors();
}

void HaikuMainWindow::PopulateTaskErrors() {
    task_error_list_->MakeEmpty();
    task_error_entries_.clear();

    for (const auto& error : shell_host_.Tasks().Errors()) {
        std::string summary = error.task_id + " [" + ToString(error.kind);
        if (!error.mechanism.empty()) {
            summary += "/" + error.mechanism;
        }
        summary += "]";
        task_error_entries_.push_back({summary, error.message});
        task_error_list_->AddItem(new BStringItem(summary.c_str()));
    }
    for (const auto& action : shell_host_.QueuedImapActions()) {
        if (!action.last_error.empty()) {
            const std::string summary = ActionKindLabel(action.kind) + " [" + action.id + "]";
            task_error_entries_.push_back({summary, action.last_error});
            task_error_list_->AddItem(new BStringItem(summary.c_str()));
        }
    }

    if (!task_error_entries_.empty()) {
        task_error_list_->Select(0);
    }
    UpdateTaskErrorDetail();

    const bool tasks_changed = task_entries_.size() != last_task_row_count_;
    const bool errors_changed = task_error_entries_.size() != last_error_row_count_;
    last_task_row_count_ = task_entries_.size();
    last_error_row_count_ = task_error_entries_.size();
    RefreshTaskUtilityFocus(tasks_changed, errors_changed);
}

void HaikuMainWindow::ApplyGuiPreferences() {
    if (gui_preferences_.show_toolbar) {
        toolbar_view_->Show();
    } else {
        toolbar_view_->Hide();
    }

    if (gui_preferences_.show_preview_pane) {
        preview_container_->Show();
    } else {
        preview_container_->Hide();
        CancelPreviewRead();
    }

    utility_container_->SetExplicitMinSize(
        BSize(B_SIZE_UNSET, std::max(96, gui_preferences_.utility_pane_height)));
    if (gui_preferences_.utility_pane_open) {
        utility_container_->Show();
    } else {
        utility_container_->Hide();
    }
    SelectUtilityTab(gui_preferences_.utility_pane_selected_tab);
    UpdateViewMenuMarks();
}

void HaikuMainWindow::PersistGuiPreferences() {
    gui_preferences_.utility_pane_open = utility_container_ != nullptr && !utility_container_->IsHidden();
    gui_preferences_.show_preview_pane = preview_container_ != nullptr && !preview_container_->IsHidden();
    gui_preferences_.show_toolbar = toolbar_view_ != nullptr && !toolbar_view_->IsHidden();
    if (utility_tabs_ != nullptr) {
        gui_preferences_.utility_pane_selected_tab = utility_tabs_->Selection();
        gui_preferences_.utility_pane_height =
            std::max(96, static_cast<int>(std::lround(utility_tabs_->Frame().Height())));
    }
    ApplyGuiPreferencesToSettings(gui_preferences_, shell_host_.Settings());
    std::string ignored;
    shell_host_.PersistSettings(&ignored);
}

void HaikuMainWindow::UpdateViewMenuMarks() {
    if (show_toolbar_item_ != nullptr) {
        show_toolbar_item_->SetMarked(gui_preferences_.show_toolbar);
    }
    if (show_preview_item_ != nullptr) {
        show_preview_item_->SetMarked(gui_preferences_.show_preview_pane);
    }
    if (show_utility_item_ != nullptr) {
        show_utility_item_->SetMarked(gui_preferences_.utility_pane_open);
    }
}

void HaikuMainWindow::TogglePreviewPane() {
    gui_preferences_.show_preview_pane = !gui_preferences_.show_preview_pane;
    ApplyGuiPreferences();
    PersistGuiPreferences();
}

void HaikuMainWindow::ToggleToolbar() {
    gui_preferences_.show_toolbar = !gui_preferences_.show_toolbar;
    ApplyGuiPreferences();
    PersistGuiPreferences();
}

void HaikuMainWindow::ToggleUtilityPane() {
    gui_preferences_.utility_pane_open = !gui_preferences_.utility_pane_open;
    ApplyGuiPreferences();
    PersistGuiPreferences();
}

void HaikuMainWindow::SelectUtilityTab(int32 index) {
    if (utility_tabs_ == nullptr || utility_tabs_->CountTabs() == 0) {
        return;
    }
    const int32 max_index = std::max<int32>(0, utility_tabs_->CountTabs() - 1);
    const int32 clamped = std::max<int32>(0, std::min<int32>(index, max_index));
    utility_tabs_->Select(clamped);
    gui_preferences_.utility_pane_selected_tab = clamped;
    UpdateViewMenuMarks();
}

void HaikuMainWindow::SetStatusMessage(std::string message) {
    if (status_view_ != nullptr) {
        status_view_->SetText(message.c_str());
    }
}

void HaikuMainWindow::RefreshTaskUtilityFocus(bool tasks_changed, bool errors_changed) {
    if (errors_changed && !task_error_entries_.empty() && gui_preferences_.bring_task_error_to_front) {
        gui_preferences_.utility_pane_open = true;
        ApplyGuiPreferences();
        SelectUtilityTab(1);
        Activate(true);
        return;
    }
    if (tasks_changed && !task_entries_.empty() && gui_preferences_.bring_task_status_to_front) {
        gui_preferences_.utility_pane_open = true;
        ApplyGuiPreferences();
        SelectUtilityTab(0);
        Activate(true);
    }
}

void HaikuMainWindow::SchedulePreviewRead() {
    CancelPreviewRead();
    if (!gui_preferences_.show_preview_pane || !gui_preferences_.mark_previewed_read ||
        gui_preferences_.preview_read_seconds <= 0) {
        return;
    }
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || !detail->unread || detail->mailbox_id == "drafts") {
        return;
    }
    preview_read_runner_ = std::make_unique<BMessageRunner>(BMessenger(this),
                                                            new BMessage(kPreviewReadTickMessage),
                                                            gui_preferences_.preview_read_seconds * 1000000LL,
                                                            1);
}

void HaikuMainWindow::MarkSelectedMessageReadFromPreview() {
    preview_read_runner_.reset();
    if (!gui_preferences_.mark_previewed_read || current_message_id_.empty()) {
        return;
    }
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || !detail->unread || detail->mailbox_id == "drafts") {
        return;
    }
    auto record = shell_host_.Messages().GetMessage(detail->mailbox_id, detail->id);
    if (!record) {
        return;
    }
    record->unread = false;
    record->updated_at = std::time(nullptr);
    std::string ignored;
    if (!shell_host_.Messages().SaveMessage(*record, &ignored)) {
        return;
    }
    shell_host_.ReloadWorkspace();
    SetStatusMessage("Marked previewed message as read.");
}

void HaikuMainWindow::CancelPreviewRead() {
    preview_read_runner_.reset();
}

void HaikuMainWindow::UpdateTaskErrorDetail() {
    const int32 index = task_error_list_->CurrentSelection();
    if (index < 0 || static_cast<std::size_t>(index) >= task_error_entries_.size()) {
        task_error_detail_->SetText("No task errors.");
        return;
    }
    task_error_detail_->SetText(task_error_entries_[static_cast<std::size_t>(index)].detail.c_str());
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
        shell_host_.RecordAttachmentLaunch(detail->attachments[attachment_index].name,
                                           *path,
                                           detail->mailbox_id + ":" + detail->id);
        SetStatusMessage("Opened selected attachment.");
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

    SetStatusMessage("Unable to open the selected attachment.");
    SelectUtilityTab(1);
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
        SetStatusMessage("Unable to save the selected attachment.");
        SelectUtilityTab(1);
        return;
    }

    SetStatusMessage("Attachment saved to " + destination.string());
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
        SetStatusMessage("Unable to save all attachments.");
        SelectUtilityTab(1);
        return;
    }

    SetStatusMessage("Saved all attachments to " + destination.string());
}

void HaikuMainWindow::HandleFetchSelectedAttachment() {
    const int32 selection = attachment_list_->CurrentSelection();
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || selection < 0 || static_cast<std::size_t>(selection) >= attachment_indices_.size()) {
        return;
    }

    const std::size_t attachment_index = attachment_indices_[static_cast<std::size_t>(selection)];
    const bool success =
        shell_host_.FetchAttachment(detail->mailbox_id, detail->id, attachment_index) && shell_host_.CheckMail();
    PopulateTaskStatus();
    SetStatusMessage(success ? "Attachment fetch queued." : "Unable to fetch the selected attachment.");
}

void HaikuMainWindow::HandleFetchSelectedMessage() {
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail) {
        return;
    }
    const bool success = shell_host_.FetchFullMessage(detail->mailbox_id, detail->id) && shell_host_.CheckMail();
    PopulateTaskStatus();
    SetStatusMessage(success ? "Full message fetch queued." : "Unable to fetch the full message.");
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

    const bool queued = shell_host_.DeleteRemoteMailbox(mailbox->id);
    PopulateTaskStatus();
    SetStatusMessage(queued ? "Remote mailbox deletion queued." :
                              "Unable to queue remote mailbox deletion.");
}

void HaikuMainWindow::RefreshWorkspace() {
    PopulateWorkspace();
    PopulateTaskStatus();
    ApplyGuiPreferences();
}

}  // namespace hermes::haiku_port
