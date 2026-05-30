#include "HaikuShellHost.h"

#include <Application.h>
#include <Alert.h>
#include <Button.h>
#include <Entry.h>
#include <FilePanel.h>
#include <GroupView.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <OutlineListView.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <private/interface/ColumnListView.h>
#include <private/interface/ColumnTypes.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>

#include "HaikuComposeWindow.h"
#include "HaikuMainWindow.h"
#include "HaikuMessageWindow.h"
#include "HaikuWazooWindow.h"
#include "hermes/FilterEngine.h"
#include "hermes/HemeraIdentity.h"
#include "hermes/MailboxMaintenance.h"

namespace hemera::haiku {

namespace {

std::filesystem::path SourceRoot() {
#ifdef HERMES_SOURCE_ROOT
    return std::filesystem::path(HERMES_SOURCE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

constexpr uint32_t kHelpContentsSelectionMessage = 'hcts';
constexpr uint32_t kHelpTopicSelectionMessage = 'htpc';
constexpr uint32_t kHelpOpenSourceMessage = 'hops';
constexpr uint32_t kHelpRevealSourceMessage = 'hrvs';
constexpr uint32_t kImportChooseRootMessage = 'imrt';
constexpr uint32_t kImportPreviewMessage = 'impr';
constexpr uint32_t kImportRunMessage = 'imrn';
constexpr uint32_t kImportSourceChosenMessage = 'imsc';
constexpr uint32_t kImportSnapshotChosenMessage = 'imss';

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

bool HasPluginExtension(const std::filesystem::path& path) {
#if defined(_WIN32)
    return path.extension() == ".dll";
#elif defined(__APPLE__)
    return path.extension() == ".bundle" || path.extension() == ".dylib" || path.extension() == ".so";
#else
    return path.extension() == ".so";
#endif
}

std::string SerializeFrame(BRect frame) {
    std::ostringstream stream;
    stream << frame.left << ',' << frame.top << ',' << frame.right << ',' << frame.bottom;
    return stream.str();
}

std::optional<BRect> ParseFrame(std::string_view serialized) {
    std::istringstream stream(std::string(serialized));
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    char c1 = '\0';
    char c2 = '\0';
    char c3 = '\0';
    if ((stream >> left >> c1 >> top >> c2 >> right >> c3 >> bottom) && c1 == ',' && c2 == ',' &&
        c3 == ',') {
        return BRect(left, top, right, bottom);
    }
    return std::nullopt;
}

std::string SerializeSplitWeights(BSplitView& split_view) {
    std::ostringstream stream;
    bool first = true;
    for (int32 index = 0; index < split_view.CountChildren(); ++index) {
        if (!first) {
            stream << ',';
        }
        first = false;
        stream << split_view.ItemWeight(index);
    }
    return stream.str();
}

void RestoreSplitWeights(BSplitView& split_view, std::string_view serialized) {
    if (serialized.empty()) {
        return;
    }
    std::istringstream stream(std::string(serialized));
    std::string token;
    int32 index = 0;
    while (std::getline(stream, token, ',') && index < split_view.CountChildren()) {
        try {
            split_view.SetItemWeight(index, std::stof(token));
        } catch (...) {
            return;
        }
        ++index;
    }
}

std::string ImportArtifactKindLabel(ImportArtifactKind kind) {
    switch (kind) {
        case ImportArtifactKind::kSettingsSnapshot:
            return "Settings";
        case ImportArtifactKind::kHelpTopicMap:
            return "Help Topic Map";
        case ImportArtifactKind::kHelpContents:
            return "Help Contents";
        case ImportArtifactKind::kImportConfig:
            return "Import Config";
        case ImportArtifactKind::kOther:
            return "Other";
    }
    return "Other";
}

std::string PreviewText(std::string_view body, std::size_t limit = 80) {
    return std::string(body.substr(0, std::min<std::size_t>(body.size(), limit)));
}

std::string SanitizeId(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(ch);
        } else {
            result.push_back('-');
        }
    }
    return result;
}

std::string MailboxProtocolLabel(MailboxProtocol protocol) {
    switch (protocol) {
        case MailboxProtocol::kLocal:
            return "local";
        case MailboxProtocol::kPop:
            return "pop";
        case MailboxProtocol::kImap:
            return "imap";
        case MailboxProtocol::kSmtp:
            return "smtp";
    }
    return "local";
}

int MaxRecentMailboxCount(const SettingsStore& settings) {
    return std::clamp(settings.GetInt("Settings", "MaxRecentMailbox", 10), 0, 99);
}

std::string DeliveryStateLabel(MessageDeliveryState state) {
    switch (state) {
        case MessageDeliveryState::kDraft:
            return "draft";
        case MessageDeliveryState::kQueued:
            return "queued";
        case MessageDeliveryState::kSending:
            return "sending";
        case MessageDeliveryState::kSent:
            return "sent";
        case MessageDeliveryState::kReceived:
            return "received";
        case MessageDeliveryState::kFailed:
            return "failed";
    }
    return "received";
}

std::string PriorityLabel(ComposePriority priority) {
    switch (priority) {
        case ComposePriority::kHighest:
            return "highest";
        case ComposePriority::kHigh:
            return "high";
        case ComposePriority::kNormal:
            return "normal";
        case ComposePriority::kLow:
            return "low";
        case ComposePriority::kLowest:
            return "lowest";
    }
    return "normal";
}

std::string TrimWhitespace(std::string_view value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return std::string(value.substr(start, end - start));
}

std::string ExtractEmailAddress(std::string_view value) {
    const std::size_t lt = value.find('<');
    const std::size_t gt = value.find('>', lt == std::string_view::npos ? 0 : lt + 1);
    if (lt != std::string_view::npos && gt != std::string_view::npos && gt > lt + 1) {
        return TrimWhitespace(value.substr(lt + 1, gt - lt - 1));
    }
    return TrimWhitespace(value);
}

std::string ExtractDisplayName(std::string_view value) {
    const std::size_t lt = value.find('<');
    if (lt != std::string_view::npos) {
        return TrimWhitespace(value.substr(0, lt));
    }
    return TrimWhitespace(value);
}

std::string EnsureSubjectPrefix(std::string_view prefix, std::string_view subject) {
    const std::string trimmed = TrimWhitespace(subject);
    if (trimmed.empty()) {
        return std::string(prefix);
    }
    const std::string normalized_prefix = std::string(prefix) + ":";
    if (trimmed.size() >= normalized_prefix.size() + 1 &&
        std::equal(normalized_prefix.begin(),
                   normalized_prefix.end(),
                   trimmed.begin(),
                   [](char left, char right) {
                       return std::tolower(static_cast<unsigned char>(left)) ==
                              std::tolower(static_cast<unsigned char>(right));
                   })) {
        return trimmed;
    }
    return std::string(prefix) + ": " + trimmed;
}

std::string QuotePlainText(std::string_view body) {
    std::ostringstream stream;
    std::istringstream input(std::string(body));
    std::string line;
    bool wrote_any = false;
    while (std::getline(input, line)) {
        stream << "> " << line << '\n';
        wrote_any = true;
    }
    if (!wrote_any) {
        stream << "> \n";
    }
    return stream.str();
}

std::string MailboxParentId(const MailboxRecord& mailbox) {
    if (mailbox.account_id.empty()) {
        return {};
    }

    if (mailbox.protocol == MailboxProtocol::kImap && !mailbox.remote_name.empty()) {
        const std::size_t split = mailbox.remote_name.find_last_of("/.");
        if (split != std::string::npos && split > 0) {
            return mailbox.account_id + ":" + SanitizeId(mailbox.remote_name.substr(0, split));
        }
    }

    return "account:" + mailbox.account_id;
}

AttachmentSummary BuildAttachmentSummary(const MessageAttachment& attachment) {
    AttachmentSummary summary;
    summary.name = attachment.name;
    summary.content_type = attachment.content_type;
    summary.size = attachment.size;
    summary.omitted = attachment.omitted;
    summary.download_complete = attachment.download_complete;
    summary.fetch_error = attachment.fetch_error;
    summary.content_id = attachment.content_id;
    summary.disposition = attachment.disposition;
    return summary;
}

AttachmentSummary BuildAttachmentSummary(const ComposeAttachment& attachment) {
    AttachmentSummary summary;
    summary.name = attachment.display_name.empty() ? attachment.source_path.filename().string()
                                                   : attachment.display_name;
    summary.content_type = attachment.mime_type;
    summary.size = static_cast<std::size_t>(attachment.size);
    summary.omitted = false;
    summary.download_complete = true;
    summary.content_id = attachment.content_id;
    summary.disposition = attachment.inline_disposition ? "inline" : "attachment";
    return summary;
}

class HemeraApplication final : public BApplication {
public:
    explicit HemeraApplication(HaikuShellHost& shell_host)
        : BApplication(hermes::kHemeraAppSignature.data()),
          shell_host_(shell_host) {}

    void ReadyToRun() override {
        shell_host_.ShowMainWindow();
    }

private:
    HaikuShellHost& shell_host_;
};

const hermes::WazooWindowState& WazooStateForGroup(const GuiPreferences& preferences,
                                                   std::string_view group_id) {
    if (group_id == "mailboxes") {
        return preferences.mailboxes_wazoo;
    }
    if (group_id == "tools") {
        return preferences.tools_wazoo;
    }
    return preferences.tasks_wazoo;
}

std::string GroupIdForTool(std::string_view tool_id) {
    if (tool_id == "mailboxes" || tool_id == "file-browser" || tool_id == "signatures" ||
        tool_id == "stationery" || tool_id == "personalities") {
        return "mailboxes";
    }
    if (tool_id == "nicknames" || tool_id == "directory-services" || tool_id == "filters" ||
        tool_id == "filter-report" || tool_id == "link-history" || tool_id == "search" ||
        tool_id == "plugins") {
        return "tools";
    }
    if (tool_id == "task-status" || tool_id == "task-errors") {
        return "tasks";
    }
    return {};
}

std::string GroupTitle(std::string_view group_id) {
    if (group_id == "mailboxes") {
        return "Mailboxes";
    }
    if (group_id == "tools") {
        return "Tools";
    }
    if (group_id == "tasks") {
        return "Tasks";
    }
    return "Wazoo";
}

std::vector<HaikuWazooWindow::ToolSpec> GroupTools(std::string_view group_id) {
    using ToolSpec = HaikuWazooWindow::ToolSpec;
    if (group_id == "mailboxes") {
        return {
            ToolSpec{"mailboxes", "Mailboxes"},
            ToolSpec{"file-browser", "File Browser"},
            ToolSpec{"signatures", "Signatures"},
            ToolSpec{"stationery", "Stationery"},
            ToolSpec{"personalities", "Personalities"},
        };
    }
    if (group_id == "tools") {
        return {
            ToolSpec{"nicknames", "Nicknames"},
            ToolSpec{"directory-services", "Directory Services"},
            ToolSpec{"filters", "Filters"},
            ToolSpec{"filter-report", "Filter Report"},
            ToolSpec{"link-history", "Link History"},
            ToolSpec{"search", "Search"},
            ToolSpec{"plugins", "Plugins"},
        };
    }
    if (group_id == "tasks") {
        return {
            ToolSpec{"task-status", "Task Status"},
            ToolSpec{"task-errors", "Task Errors"},
        };
    }
    return {};
}

class HaikuHelpWindow final : public BWindow {
public:
    explicit HaikuHelpWindow(HaikuShellHost& shell_host)
        : BWindow(BRect(180, 180, 980, 760),
                  "Help Contents",
                  B_TITLED_WINDOW,
                  B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
          shell_host_(shell_host) {
        contents_view_ = new BOutlineListView("help-contents");
        contents_view_->SetSelectionMessage(new BMessage(kHelpContentsSelectionMessage));
        topics_view_ = new BListView("help-topics");
        topics_view_->SetSelectionMessage(new BMessage(kHelpTopicSelectionMessage));
        detail_view_ = new BTextView("help-detail");
        detail_view_->MakeEditable(false);
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);

        auto* open_button = new BButton("help-open-source", "Open Source", new BMessage(kHelpOpenSourceMessage));
        auto* reveal_button = new BButton("help-reveal-source",
                                          "Reveal Source",
                                          new BMessage(kHelpRevealSourceMessage));

        auto* right_side = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(right_side, B_VERTICAL, 8)
            .Add(new BStringView("help-topics-label", "Topics"))
            .Add(new BScrollView("help-topics-scroll", topics_view_, 0, false, true), 0.38f)
            .Add(new BStringView("help-detail-label", "Details"))
            .Add(new BScrollView("help-detail-scroll", detail_view_, 0, false, true), 0.62f)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(open_button)
                .Add(reveal_button)
                .AddGlue()
            .End();

        split_view_ = new BSplitView(B_HORIZONTAL);
        split_view_->AddChild(new BScrollView("help-contents-scroll", contents_view_, 0, false, true));
        split_view_->AddChild(right_side);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 0).Add(split_view_);
        RefreshCatalog();
    }

    bool QuitRequested() override {
        PersistState();
        Hide();
        return false;
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kHelpContentsSelectionMessage:
                SelectTopicFromContents();
                return;

            case kHelpTopicSelectionMessage:
                UpdateDetailFromSelection();
                return;

            case kHelpOpenSourceMessage:
                OpenSelectedSource(false);
                return;

            case kHelpRevealSourceMessage:
                OpenSelectedSource(true);
                return;

            default:
                BWindow::MessageReceived(message);
                return;
        }
    }

    void RefreshCatalog() {
        contents_view_->MakeEmpty();
        topics_view_->MakeEmpty();
        contents_.clear();
        topics_.clear();

        contents_ = shell_host_.Help().Contents();
        topics_ = shell_host_.Help().Topics();
        std::sort(topics_.begin(),
                  topics_.end(),
                  [](const HelpTopic& left, const HelpTopic& right) { return left.label < right.label; });

        for (const auto& entry : contents_) {
            const int32 outline_level = std::max(0, entry.level - 1);
            contents_view_->AddItem(new BStringItem(entry.label.c_str(), outline_level, false));
        }
        for (const auto& topic : topics_) {
            topics_view_->AddItem(new BStringItem(topic.label.c_str()));
        }

        const auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        if (const auto frame = ParseFrame(preferences.help_window_frame)) {
            MoveTo(frame->LeftTop());
            ResizeTo(frame->Width(), frame->Height());
        }
        RestoreSplitWeights(*split_view_, preferences.help_split_layout);

        if (!preferences.help_selected_topic_id.empty()) {
            SelectTopicById(preferences.help_selected_topic_id);
        } else if (!topics_.empty()) {
            topics_view_->Select(0L);
            UpdateDetailFromSelection();
        } else {
            detail_view_->SetText("No help topics are currently available.");
        }
    }

private:
    void PersistState() {
        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        preferences.help_window_frame = SerializeFrame(Frame());
        preferences.help_split_layout = SerializeSplitWeights(*split_view_);
        preferences.help_selected_topic_id = SelectedTopicId();
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }

    std::string SelectedTopicId() const {
        const int32 index = topics_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= topics_.size()) {
            return {};
        }
        return topics_[static_cast<std::size_t>(index)].id;
    }

    void SelectTopicById(std::string_view topic_id) {
        for (std::size_t index = 0; index < topics_.size(); ++index) {
            if (topics_[index].id == topic_id) {
                topics_view_->Select(static_cast<int32>(index));
                UpdateDetailFromSelection();
                return;
            }
        }
    }

    void SelectTopicFromContents() {
        const int32 index = contents_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= contents_.size()) {
            return;
        }
        const auto& entry = contents_[static_cast<std::size_t>(index)];
        if (!entry.topic_id.empty()) {
            SelectTopicById(entry.topic_id);
        } else {
            detail_view_->SetText(entry.label.c_str());
        }
    }

    void UpdateDetailFromSelection() {
        const int32 index = topics_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= topics_.size()) {
            detail_view_->SetText("Select a help topic.");
            return;
        }
        const auto& topic = topics_[static_cast<std::size_t>(index)];
        std::ostringstream detail;
        detail << "Label: " << topic.label << '\n'
               << "Topic ID: " << topic.id << '\n'
               << "Source: " << topic.source_path.string();
        detail_view_->SetText(detail.str().c_str());
    }

    void OpenSelectedSource(bool reveal_parent) {
        const int32 index = topics_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= topics_.size()) {
            return;
        }
        auto source_path = topics_[static_cast<std::size_t>(index)].source_path;
        if (reveal_parent && !source_path.empty()) {
            source_path = source_path.parent_path();
        }
        if (source_path.empty() || !LaunchPath(source_path)) {
            BAlert("help-source", "Unable to open the selected help source.", "OK")->Go();
        }
    }

    HaikuShellHost& shell_host_;
    BSplitView* split_view_ = nullptr;
    BOutlineListView* contents_view_ = nullptr;
    BListView* topics_view_ = nullptr;
    BTextView* detail_view_ = nullptr;
    std::vector<HelpContentsEntry> contents_;
    std::vector<HelpTopic> topics_;
};

class ImportArtifactRow final : public BRow {
public:
    explicit ImportArtifactRow(const ImportArtifact& artifact)
        : artifact_(artifact) {
        SetField(new BStringField(ImportArtifactKindLabel(artifact.kind).c_str()), 0);
        SetField(new BStringField(artifact.source_path.string().c_str()), 1);
        SetField(new BStringField(artifact.relative_destination.string().c_str()), 2);
    }

    ImportArtifact artifact_;
};

class HaikuImportWindow final : public BWindow {
public:
    explicit HaikuImportWindow(HaikuShellHost& shell_host)
        : BWindow(BRect(220, 220, 1080, 760),
                  "Import from Eudora",
                  B_TITLED_WINDOW,
                  B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
          shell_host_(shell_host) {
        source_root_control_ = new BTextControl("import-source-root", "Source Root:", "", nullptr);
        auto* browse_button =
            new BButton("import-browse", "Choose" B_UTF8_ELLIPSIS, new BMessage(kImportChooseRootMessage));
        auto* preview_button =
            new BButton("import-preview", "Preview", new BMessage(kImportPreviewMessage));
        auto* import_button =
            new BButton("import-run", "Import", new BMessage(kImportRunMessage));

        auto* snapshot_menu = new BPopUpMenu("import-snapshots");
        snapshot_menu_field_ = new BMenuField("import-snapshot", "Settings Snapshot:", snapshot_menu);

        artifacts_view_ =
            new BColumnListView("import-artifacts", B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS);
        artifacts_view_->SetSelectionMode(B_SINGLE_SELECTION_LIST);
        artifacts_view_->AddColumn(
            new BStringColumn("Type", 144.0f, 96.0f, 220.0f, B_TRUNCATE_END), 0);
        artifacts_view_->AddColumn(
            new BStringColumn("Source Path", 360.0f, 180.0f, 640.0f, B_TRUNCATE_MIDDLE), 1);
        artifacts_view_->AddColumn(
            new BStringColumn("Destination", 220.0f, 120.0f, 420.0f, B_TRUNCATE_MIDDLE), 2);

        status_view_ = new BTextView("import-status");
        status_view_->MakeEditable(false);
        status_view_->SetWordWrap(true);
        status_view_->SetInsets(8, 8, 8, 8);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(source_root_control_, 1.0f)
                .Add(browse_button)
                .Add(preview_button)
            .End()
            .Add(snapshot_menu_field_)
            .Add(new BScrollView("import-artifacts-scroll", artifacts_view_, 0, false, true), 0.66f)
            .Add(new BStringView("import-status-label", "Status"))
            .Add(new BScrollView("import-status-scroll", status_view_, 0, false, true), 0.34f)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(import_button)
                .AddGlue()
            .End();

        const auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        source_root_control_->SetText(preferences.import_last_source_root.c_str());
        if (const auto frame = ParseFrame(preferences.import_window_frame)) {
            MoveTo(frame->LeftTop());
            ResizeTo(frame->Width(), frame->Height());
        }
        if (!preferences.import_last_source_root.empty()) {
            PreviewArtifacts();
            SelectSnapshotByName(preferences.import_selected_settings_snapshot);
        } else {
            status_view_->SetText("Choose a legacy Eudora source root, preview the artifacts, then import.");
        }
    }

    bool QuitRequested() override {
        PersistState();
        Hide();
        return false;
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kImportChooseRootMessage:
                OpenSourceChooser();
                return;

            case kImportPreviewMessage:
                PreviewArtifacts();
                return;

            case kImportRunMessage:
                RunImport();
                return;

            case kImportSourceChosenMessage: {
                entry_ref ref;
                if (message->FindRef("refs", &ref) != B_OK) {
                    return;
                }
                BEntry entry(&ref, true);
                BPath path;
                if (entry.GetPath(&path) == B_OK) {
                    source_root_control_->SetText(path.Path());
                    PreviewArtifacts();
                }
                return;
            }

            case kImportSnapshotChosenMessage:
                PersistState();
                return;

            default:
                BWindow::MessageReceived(message);
                return;
        }
    }

private:
    void PersistState() {
        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        preferences.import_window_frame = SerializeFrame(Frame());
        preferences.import_last_source_root = source_root_control_->Text();
        preferences.import_selected_settings_snapshot = SelectedSnapshotName();
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }

    void OpenSourceChooser() {
        if (!source_panel_) {
            source_panel_ = std::make_unique<BFilePanel>(B_OPEN_PANEL,
                                                         new BMessenger(this),
                                                         nullptr,
                                                         B_DIRECTORY_NODE,
                                                         false,
                                                         new BMessage(kImportSourceChosenMessage),
                                                         nullptr,
                                                         false,
                                                         true);
        }
        source_panel_->Show();
    }

    std::string SelectedSnapshotName() const {
        if (snapshot_menu_field_ == nullptr || snapshot_menu_field_->Menu() == nullptr ||
            snapshot_menu_field_->Menu()->FindMarked() == nullptr) {
            return {};
        }
        return snapshot_menu_field_->Menu()->FindMarked()->Label();
    }

    void SelectSnapshotByName(std::string_view snapshot_name) {
        if (snapshot_menu_field_ == nullptr || snapshot_menu_field_->Menu() == nullptr) {
            return;
        }
        for (int32 index = 0; index < snapshot_menu_field_->Menu()->CountItems(); ++index) {
            if (BMenuItem* item = snapshot_menu_field_->Menu()->ItemAt(index); item != nullptr) {
                item->SetMarked(item->Label() == snapshot_name);
            }
        }
    }

    void PreviewArtifacts() {
        artifacts_.clear();
        artifacts_view_->Clear();
        if (snapshot_menu_field_->Menu() != nullptr) {
            snapshot_menu_field_->Menu()->RemoveItems(0, snapshot_menu_field_->Menu()->CountItems(), true);
        }

        const std::filesystem::path source_root = source_root_control_->Text();
        if (source_root.empty()) {
            status_view_->SetText("Choose a source root to preview importable artifacts.");
            return;
        }

        artifacts_ = shell_host_.Importer().Discover(source_root);
        std::vector<std::string> snapshot_names;
        for (const auto& artifact : artifacts_) {
            artifacts_view_->AddRow(new ImportArtifactRow(artifact));
            if (artifact.kind == ImportArtifactKind::kSettingsSnapshot) {
                snapshot_names.push_back(artifact.relative_destination.filename().string());
            }
        }

        for (const auto& name : snapshot_names) {
            snapshot_menu_field_->Menu()->AddItem(
                new BMenuItem(name.c_str(), new BMessage(kImportSnapshotChosenMessage)));
        }
        if (!snapshot_names.empty()) {
            const auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
            const auto remembered = preferences.import_selected_settings_snapshot;
            if (!remembered.empty()) {
                SelectSnapshotByName(remembered);
            }
            if (snapshot_menu_field_->Menu()->FindMarked() == nullptr) {
                if (BMenuItem* first = snapshot_menu_field_->Menu()->ItemAt(0); first != nullptr) {
                    first->SetMarked(true);
                }
            }
        }

        std::ostringstream status;
        status << "Discovered " << artifacts_.size() << " importable artifact";
        if (artifacts_.size() != 1) {
            status << 's';
        }
        status << " in " << source_root.string() << '.';
        if (snapshot_names.empty()) {
            status << "\nNo Eudora settings snapshot was found.";
        } else if (snapshot_names.size() == 1) {
            status << "\nSettings snapshot: " << snapshot_names.front();
        } else {
            status << "\nSelect the settings snapshot that should become the live EUDORA.ini.";
        }
        status_view_->SetText(status.str().c_str());
        PersistState();
    }

    void RunImport() {
        const std::filesystem::path source_root = source_root_control_->Text();
        if (source_root.empty()) {
            status_view_->SetText("Choose a source root before importing.");
            return;
        }

        std::string error_message;
        if (!shell_host_.Importer().Import(source_root, shell_host_.DataRootPath(), &error_message)) {
            status_view_->SetText(
                error_message.empty() ? "Import failed." : error_message.c_str());
            return;
        }

        const auto snapshot_name = SelectedSnapshotName();
        if (!shell_host_.ReloadImportedShellState(snapshot_name, &error_message)) {
            status_view_->SetText(
                error_message.empty() ? "Import completed, but reload failed." : error_message.c_str());
            return;
        }

        std::ostringstream status;
        status << "Imported artifacts from " << source_root.string() << '.';
        if (!snapshot_name.empty()) {
            status << "\nApplied settings snapshot: " << snapshot_name;
        }
        status << "\nThe shell state has been reloaded.";
        status_view_->SetText(status.str().c_str());
        PreviewArtifacts();
    }

    HaikuShellHost& shell_host_;
    BTextControl* source_root_control_ = nullptr;
    BMenuField* snapshot_menu_field_ = nullptr;
    BColumnListView* artifacts_view_ = nullptr;
    BTextView* status_view_ = nullptr;
    std::unique_ptr<BFilePanel> source_panel_;
    std::vector<ImportArtifact> artifacts_;
};

}  // namespace

HaikuShellHost::HaikuShellHost()
    : settings_(std::make_unique<IniSettingsStore>()),
      workspace_(std::make_unique<InMemoryWorkspaceModel>()),
      account_service_(std::make_unique<LegacyAccountService>()),
      credential_store_(std::make_unique<FilesystemCredentialStore>(DataRoot())),
      oauth_token_store_(std::make_unique<FilesystemOAuthTokenStore>(DataRoot())),
      sync_state_store_(std::make_unique<FilesystemSyncStateStore>(DataRoot())),
      imap_action_store_(std::make_unique<FilesystemImapActionStore>(DataRoot())),
      task_model_(std::make_unique<InMemoryMailTaskModel>()),
      draft_store_(std::make_unique<FilesystemDraftStore>(DataRoot())),
      mailbox_store_(std::make_unique<FilesystemMailboxStore>(DataRoot())),
      message_store_(std::make_unique<FilesystemMessageStore>(DataRoot())),
      nickname_store_(std::make_unique<FlatFileNicknameStore>()),
      stationery_store_(std::make_unique<FilesystemStationeryStore>()),
      signature_store_(std::make_unique<FilesystemSignatureStore>()),
      address_book_service_(std::make_unique<MemoryAddressBookService>()),
      filter_store_(std::make_unique<FilesystemFilterStore>()),
      filter_report_store_(std::make_unique<FilesystemFilterReportStore>()),
      link_history_store_(std::make_unique<FilesystemLinkHistoryStore>()),
      plugin_host_(std::make_unique<FilesystemPluginHost>()),
      directory_services_(std::make_unique<LocalDirectoryServiceCatalog>(nickname_store_.get(),
                                                                         address_book_service_.get())),
      help_catalog_(std::make_unique<LegacyHelpCatalog>()),
      import_service_(std::make_unique<LegacyImportService>()),
      search_service_(std::make_unique<SimpleSearchService>()),
      tls_provider_(std::make_unique<OpenSslTlsProvider>()),
      transport_service_(std::make_unique<SocketTransportService>(tls_provider_.get())),
      oauth_http_client_(std::make_unique<TransportOAuthHttpClient>(*transport_service_, *tls_provider_)),
      oauth_device_flow_service_(std::make_unique<OAuthDeviceFlowService>(*oauth_http_client_,
                                                                          *oauth_token_store_,
                                                                          *credential_store_)),
      transport_coordinator_(std::make_unique<MailTransportCoordinator>(*account_service_,
                                                                        *credential_store_,
                                                                        *sync_state_store_,
                                                                        *mailbox_store_,
                                                                        *message_store_,
                                                                        *transport_service_,
                                                                        *tls_provider_,
                                                                        *task_model_,
                                                                        oauth_device_flow_service_.get(),
                                                                        oauth_token_store_.get(),
                                                                        imap_action_store_.get())),
      paige_runtime_(std::make_unique<PaigeRuntime>()) {
    std::string ignored;
    (void)paige_runtime_->Initialize(&ignored);
    EnsureWorkspaceDirectories();
    LoadBootstrapAccounts();
    LoadToolData();
    ApplyPendingFilters();
    ReloadHelpCatalog();
    RescanPlugins();
    ReloadWorkspace();
}

int HaikuShellHost::Run() {
    HemeraApplication app(*this);
    app.Run();
    return 0;
}

bool HaikuShellHost::OpenMailbox(std::string_view mailbox_id) {
    active_mailbox_id_ = std::string(mailbox_id);
    RememberRecentMailbox(mailbox_id);
    if (main_window_) {
        main_window_->SetCurrentMailbox(active_mailbox_id_, false);
    }
    return true;
}

bool HaikuShellHost::OpenComposer(const ComposeMessage& message) {
    if (!main_window_) {
        pending_composer_message_ = message;
        return true;
    }

    ShowComposeWindow(message);
    return true;
}

bool HaikuShellHost::SendQueued() {
    if (ShellBehavior().offline) {
        return false;
    }
    const auto summary = transport_coordinator_->SendQueued();
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::CheckMail() {
    if (ShellBehavior().offline) {
        return false;
    }
    const auto summary = transport_coordinator_->CheckMail();
    ApplyPendingFilters();
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::SendAndReceive() {
    if (ShellBehavior().offline) {
        return false;
    }
    const auto summary = transport_coordinator_->SendAndReceive();
    ApplyPendingFilters();
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::RefreshMailbox(std::string_view mailbox_id) {
    if (ShellBehavior().offline) {
        return false;
    }
    const auto summary = transport_coordinator_->RefreshMailbox(mailbox_id, false);
    ApplyPendingFilters();
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::ResyncMailbox(std::string_view mailbox_id) {
    if (ShellBehavior().offline) {
        return false;
    }
    const auto summary = transport_coordinator_->RefreshMailbox(mailbox_id, true);
    ApplyPendingFilters();
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::DeleteMessage(std::string_view mailbox_id, std::string_view message_id) {
    if (ShellBehavior().offline) {
        return false;
    }
    std::string error_message;
    const bool queued = transport_coordinator_->QueueDeleteMessage(mailbox_id, message_id, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::UndeleteMessage(std::string_view mailbox_id, std::string_view message_id) {
    if (ShellBehavior().offline) {
        return false;
    }
    std::string error_message;
    const bool queued = transport_coordinator_->QueueUndeleteMessage(mailbox_id, message_id, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::PurgeMailbox(std::string_view mailbox_id) {
    if (ShellBehavior().offline) {
        return false;
    }
    std::string error_message;
    const bool queued = transport_coordinator_->QueueExpungeMailbox(mailbox_id, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::MoveMessage(std::string_view mailbox_id,
                                 std::string_view message_id,
                                 std::string_view destination_mailbox_id) {
    if (ShellBehavior().offline) {
        return false;
    }
    std::string error_message;
    const bool queued = transport_coordinator_->QueueMoveMessage(mailbox_id,
                                                                 message_id,
                                                                 destination_mailbox_id,
                                                                 &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::CopyMessage(std::string_view mailbox_id,
                                 std::string_view message_id,
                                 std::string_view destination_mailbox_id) {
    if (ShellBehavior().offline) {
        return false;
    }
    std::string error_message;
    const bool queued = transport_coordinator_->QueueCopyMessage(mailbox_id,
                                                                 message_id,
                                                                 destination_mailbox_id,
                                                                 &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::CreateRemoteMailbox(std::string_view account_id, std::string_view remote_name) {
    if (ShellBehavior().offline) {
        return false;
    }
    std::string error_message;
    const bool queued = transport_coordinator_->QueueCreateMailbox(account_id, remote_name, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::RenameRemoteMailbox(std::string_view mailbox_id, std::string_view new_remote_name) {
    if (ShellBehavior().offline) {
        return false;
    }
    std::string error_message;
    const bool queued = transport_coordinator_->QueueRenameMailbox(mailbox_id, new_remote_name, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::DeleteRemoteMailbox(std::string_view mailbox_id) {
    if (ShellBehavior().offline) {
        return false;
    }
    std::string error_message;
    const bool queued = transport_coordinator_->QueueDeleteMailbox(mailbox_id, &error_message);
    ReloadWorkspace();
    return queued;
}

std::optional<std::filesystem::path> HaikuShellHost::AttachmentPath(std::string_view mailbox_id,
                                                                    std::string_view message_id,
                                                                    std::size_t attachment_index) const {
    if (mailbox_id == "drafts") {
        const auto draft = draft_store_->GetDraft(message_id);
        if (!draft || attachment_index >= draft->attachments.size()) {
            return std::nullopt;
        }
        return draft->attachments[attachment_index].source_path;
    }
    return message_store_->AttachmentPath(mailbox_id, message_id, attachment_index);
}

bool HaikuShellHost::SaveAttachment(std::string_view mailbox_id,
                                    std::string_view message_id,
                                    std::size_t attachment_index,
                                    const std::filesystem::path& destination_path) {
    if (mailbox_id == "drafts") {
        const auto draft = draft_store_->GetDraft(message_id);
        if (!draft || attachment_index >= draft->attachments.size() || draft->attachments[attachment_index].source_path.empty()) {
            return false;
        }
        std::error_code create_error;
        std::filesystem::create_directories(destination_path.parent_path(), create_error);
        if (create_error) {
            return false;
        }
        std::filesystem::copy_file(draft->attachments[attachment_index].source_path,
                                   destination_path,
                                   std::filesystem::copy_options::overwrite_existing,
                                   create_error);
        return !create_error;
    }
    const auto payload = message_store_->LoadAttachmentPayload(mailbox_id, message_id, attachment_index);
    if (!payload) {
        return false;
    }
    std::error_code create_error;
    std::filesystem::create_directories(destination_path.parent_path(), create_error);
    std::ofstream output(destination_path, std::ios::binary);
    if (!output.is_open()) {
        return false;
    }
    output.write(payload->data(), static_cast<std::streamsize>(payload->size()));
    return static_cast<bool>(output);
}

bool HaikuShellHost::SaveAllAttachments(std::string_view mailbox_id,
                                        std::string_view message_id,
                                        const std::filesystem::path& destination_directory) {
    if (mailbox_id == "drafts") {
        const auto draft = draft_store_->GetDraft(message_id);
        if (!draft) {
            return false;
        }
        std::error_code create_error;
        std::filesystem::create_directories(destination_directory, create_error);
        if (create_error) {
            return false;
        }
        for (std::size_t index = 0; index < draft->attachments.size(); ++index) {
            const auto filename = draft->attachments[index].display_name.empty()
                                      ? ("attachment-" + std::to_string(index))
                                      : draft->attachments[index].display_name;
            if (!SaveAttachment(mailbox_id, message_id, index, destination_directory / filename)) {
                return false;
            }
        }
        return true;
    }
    const auto message = message_store_->GetMessage(mailbox_id, message_id);
    if (!message) {
        return false;
    }
    std::error_code create_error;
    std::filesystem::create_directories(destination_directory, create_error);
    if (create_error) {
        return false;
    }
    for (std::size_t index = 0; index < message->attachments.size(); ++index) {
        const auto filename =
            message->attachments[index].name.empty() ? ("attachment-" + std::to_string(index)) : message->attachments[index].name;
        if (!SaveAttachment(mailbox_id, message_id, index, destination_directory / filename)) {
            return false;
        }
    }
    return true;
}

bool HaikuShellHost::FetchAttachment(std::string_view mailbox_id,
                                     std::string_view message_id,
                                     std::size_t attachment_index) {
    if (ShellBehavior().offline) {
        return false;
    }
    std::string error_message;
    const bool queued =
        transport_coordinator_->QueueFetchAttachment(mailbox_id, message_id, attachment_index, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::FetchFullMessage(std::string_view mailbox_id, std::string_view message_id) {
    if (ShellBehavior().offline) {
        return false;
    }
    std::string error_message;
    const bool queued =
        transport_coordinator_->QueueFetchFullMessage(mailbox_id, message_id, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::FetchDefaultImapMessages(std::string_view mailbox_id,
                                              const std::vector<std::string>& message_ids) {
    if (ShellBehavior().offline) {
        return false;
    }
    bool queued_any = false;
    std::string error_message;
    for (const auto& message_id : message_ids) {
        queued_any =
            transport_coordinator_->QueueFetchDefaultMessage(mailbox_id, message_id, &error_message) || queued_any;
    }
    ReloadWorkspace();
    return queued_any;
}

bool HaikuShellHost::ClearCachedImapMessages(std::string_view mailbox_id,
                                             const std::vector<std::string>& message_ids) {
    bool updated_any = false;
    std::string error_message;
    for (const auto& message_id : message_ids) {
        const auto mailbox = mailbox_store_->GetMailbox(mailbox_id);
        auto message = message_store_->GetMessage(mailbox_id, message_id);
        if (!mailbox || mailbox->protocol != MailboxProtocol::kImap || !message || message->remote_id.empty()) {
            continue;
        }
        MessageRecord updated = *message;
        updated.plain_text_body.clear();
        updated.html_body.clear();
        updated.rtf_body.clear();
        updated.paige_native_body.clear();
        updated.download_complete = false;
        updated.attachments_omitted = !updated.attachments.empty();
        updated.last_error.clear();
        updated.updated_at = std::time(nullptr);
        for (std::size_t index = 0; index < updated.attachments.size(); ++index) {
            if (const auto payload_path = message_store_->AttachmentPath(mailbox_id, message_id, index)) {
                std::error_code remove_error;
                std::filesystem::remove(*payload_path, remove_error);
            }
            updated.attachments[index].download_complete = false;
            updated.attachments[index].omitted = true;
            updated.attachments[index].fetch_error.clear();
            updated.attachments[index].payload_path.clear();
        }
        if (message_store_->SaveMessage(updated, &error_message)) {
            updated_any = true;
        }
    }
    if (updated_any) {
        ReloadWorkspace();
    }
    return updated_any;
}

bool HaikuShellHost::RedownloadImapMessages(std::string_view mailbox_id,
                                            const std::vector<std::string>& message_ids,
                                            bool full) {
    if (ShellBehavior().offline) {
        return false;
    }
    bool queued_any = false;
    std::string error_message;
    for (const auto& message_id : message_ids) {
        queued_any =
            transport_coordinator_->QueueRedownloadMessage(mailbox_id, message_id, full, &error_message) || queued_any;
    }
    ReloadWorkspace();
    return queued_any;
}

bool HaikuShellHost::RetryTask(std::string_view task_or_action_id) {
    if (ShellBehavior().offline) {
        return false;
    }
    std::string error_message;
    const bool retried = transport_coordinator_->RetryImapAction(task_or_action_id, &error_message);
    ReloadWorkspace();
    return retried;
}

bool HaikuShellHost::CancelTask(std::string_view task_or_action_id) {
    std::string error_message;
    const bool cancelled = transport_coordinator_->CancelImapAction(task_or_action_id, &error_message);
    ReloadWorkspace();
    return cancelled;
}

bool HaikuShellHost::StopActiveTasks() {
    return transport_coordinator_->StopActiveTasks();
}

InMemoryWorkspaceModel& HaikuShellHost::Workspace() {
    return *workspace_;
}

LegacyAccountService& HaikuShellHost::Accounts() {
    return *account_service_;
}

IniSettingsStore& HaikuShellHost::Settings() {
    return *settings_;
}

FilesystemCredentialStore& HaikuShellHost::Credentials() {
    return *credential_store_;
}

FilesystemOAuthTokenStore& HaikuShellHost::OAuthTokens() {
    return *oauth_token_store_;
}

FilesystemSyncStateStore& HaikuShellHost::SyncState() {
    return *sync_state_store_;
}

FilesystemImapActionStore& HaikuShellHost::ImapActions() {
    return *imap_action_store_;
}

InMemoryMailTaskModel& HaikuShellHost::Tasks() {
    return *task_model_;
}

FilesystemDraftStore& HaikuShellHost::Drafts() {
    return *draft_store_;
}

FilesystemMailboxStore& HaikuShellHost::Mailboxes() {
    return *mailbox_store_;
}

FilesystemMessageStore& HaikuShellHost::Messages() {
    return *message_store_;
}

FlatFileNicknameStore& HaikuShellHost::Nicknames() {
    return *nickname_store_;
}

FilesystemStationeryStore& HaikuShellHost::Stationery() {
    return *stationery_store_;
}

FilesystemSignatureStore& HaikuShellHost::Signatures() {
    return *signature_store_;
}

MemoryAddressBookService& HaikuShellHost::AddressBook() {
    return *address_book_service_;
}

FilesystemFilterStore& HaikuShellHost::Filters() {
    return *filter_store_;
}

FilesystemFilterReportStore& HaikuShellHost::FilterReport() {
    return *filter_report_store_;
}

FilesystemLinkHistoryStore& HaikuShellHost::LinkHistory() {
    return *link_history_store_;
}

FilesystemPluginHost& HaikuShellHost::Plugins() {
    return *plugin_host_;
}

LocalDirectoryServiceCatalog& HaikuShellHost::DirectoryServices() {
    return *directory_services_;
}

LegacyHelpCatalog& HaikuShellHost::Help() {
    return *help_catalog_;
}

LegacyImportService& HaikuShellHost::Importer() {
    return *import_service_;
}

SimpleSearchService& HaikuShellHost::Search() {
    return *search_service_;
}

OAuthDeviceFlowService& HaikuShellHost::OAuthService() {
    return *oauth_device_flow_service_;
}

PaigeRuntime& HaikuShellHost::Runtime() {
    return *paige_runtime_;
}

MailTransportCoordinator& HaikuShellHost::TransportCoordinator() {
    return *transport_coordinator_;
}

const std::optional<ComposeMessage>& HaikuShellHost::PendingComposerMessage() const {
    return pending_composer_message_;
}

std::optional<MessageDetail> HaikuShellHost::WorkspaceMessageDetail(std::string_view message_id) const {
    return workspace_->GetMessageDetail(message_id);
}

std::vector<ImapActionRecord> HaikuShellHost::QueuedImapActions() const {
    std::vector<ImapActionRecord> actions;
    for (const auto& action : imap_action_store_->ListActions()) {
        if (action.state == ImapActionState::kPending || action.state == ImapActionState::kFailed ||
            action.state == ImapActionState::kCancelled) {
            actions.push_back(action);
        }
    }
    return actions;
}

std::filesystem::path HaikuShellHost::DataRootPath() const {
    return DataRoot();
}

std::filesystem::path HaikuShellHost::SettingsFilePath() const {
    return SettingsPath();
}

std::string HaikuShellHost::ActiveMailboxId() const {
    return active_mailbox_id_;
}

std::string HaikuShellHost::ActiveMessageId() const {
    return active_message_id_;
}

void HaikuShellHost::SetActiveMessageContext(std::string mailbox_id, std::string message_id) {
    active_mailbox_id_ = std::move(mailbox_id);
    active_message_id_ = std::move(message_id);
}

std::optional<ComposeMessage> HaikuShellHost::BuildResponseMessage(MessageResponseKind kind,
                                                                   std::string_view mailbox_id,
                                                                   std::string_view message_id,
                                                                   std::string_view stationery_name) const {
    const auto record = message_store_->GetMessage(mailbox_id, message_id);
    if (!record) {
        return std::nullopt;
    }

    ComposeMessage compose;
    compose.id = std::string(message_id) + "-compose";
    compose.policy = ComposePolicyFromSettings(*settings_);
    compose.signature_name = compose.policy.default_signature_name;
    compose.stationery_name = std::string(stationery_name);

    if (const auto stationery = !stationery_name.empty() ? stationery_store_->Find(stationery_name) : std::nullopt) {
        compose.headers.to = stationery->headers.to;
        compose.headers.cc = stationery->headers.cc;
        compose.headers.bcc = stationery->headers.bcc;
        compose.headers.subject = stationery->headers.subject;
        compose.headers.from_persona = stationery->persona;
        compose.signature_name = stationery->signature_name.empty() ? compose.signature_name
                                                                    : stationery->signature_name;
        compose.body = stationery->body;
    }

    const std::string sender_address = ExtractEmailAddress(record->sender);
    const std::string sender_name = ExtractDisplayName(record->sender);
    const std::string quoted_body = QuotePlainText(record->plain_text_body);

    switch (kind) {
        case MessageResponseKind::kReply:
            compose.headers.to = sender_address;
            compose.headers.subject = EnsureSubjectPrefix("Re", record->subject);
            compose.body.plain_text += "\n\n" + quoted_body;
            break;
        case MessageResponseKind::kReplyAll:
            compose.headers.to = sender_address;
            compose.headers.cc = record->recipients;
            compose.headers.subject = EnsureSubjectPrefix("Re", record->subject);
            compose.body.plain_text += "\n\n" + quoted_body;
            break;
        case MessageResponseKind::kForward:
            compose.headers.subject = EnsureSubjectPrefix("Fwd", record->subject);
            compose.body.plain_text += "\n\nForwarded message from " +
                                       (sender_name.empty() ? sender_address : sender_name) + ":\n" + quoted_body;
            break;
        case MessageResponseKind::kRedirect:
            compose.headers.subject = record->subject;
            compose.body.plain_text += "\n\nRedirected message:\n" + record->plain_text_body;
            compose.body.html_fragment = record->html_body;
            compose.body.rtf_fragment = record->rtf_body;
            compose.body.paige_native_bytes = record->paige_native_body;
            compose.body.styled_source = record->styled_source;
            compose.body.fidelity = record->styled_fidelity;
            compose.attachments.reserve(record->attachments.size());
            for (const auto& attachment : record->attachments) {
                ComposeAttachment compose_attachment;
                compose_attachment.display_name = attachment.name;
                compose_attachment.mime_type = attachment.content_type;
                compose_attachment.size = attachment.size;
                compose_attachment.content_id = attachment.content_id;
                compose_attachment.inline_disposition = attachment.disposition == "inline";
                if (!attachment.payload_path.empty()) {
                    compose_attachment.source_path = attachment.payload_path;
                }
                compose.attachments.push_back(std::move(compose_attachment));
            }
            break;
        case MessageResponseKind::kSendAgain:
            compose.headers.to = record->recipients;
            compose.headers.subject = record->subject;
            compose.body.plain_text = record->plain_text_body;
            compose.body.html_fragment = record->html_body;
            compose.body.rtf_fragment = record->rtf_body;
            compose.body.paige_native_bytes = record->paige_native_body;
            compose.body.styled_source = record->styled_source;
            compose.body.fidelity = record->styled_fidelity;
            compose.options = record->compose_options;
            break;
    }

    if (compose.body.plain_text.empty()) {
        compose.body.plain_text = record->plain_text_body;
    }
    return compose;
}

void HaikuShellHost::QueuePendingSearch(SearchRequest request) {
    pending_search_request_ = std::move(request);
}

std::optional<HaikuShellHost::SearchRequest> HaikuShellHost::TakePendingSearch() {
    auto request = pending_search_request_;
    pending_search_request_.reset();
    return request;
}

void HaikuShellHost::QueuePendingDirectoryQuery(std::string query) {
    pending_directory_query_ = std::move(query);
}

std::optional<std::string> HaikuShellHost::TakePendingDirectoryQuery() {
    auto query = pending_directory_query_;
    pending_directory_query_.reset();
    return query;
}

void HaikuShellHost::UpdateWazooWindowState(std::string_view group_id,
                                            const hermes::WazooWindowState& state) {
    auto preferences = GuiPreferencesFromSettings(*settings_);
    if (group_id == "mailboxes") {
        preferences.mailboxes_wazoo = state;
    } else if (group_id == "tools") {
        preferences.tools_wazoo = state;
    } else if (group_id == "tasks") {
        preferences.tasks_wazoo = state;
    } else {
        return;
    }
    ApplyGuiPreferencesToSettings(preferences, *settings_);
    std::string ignored;
    PersistSettings(&ignored);
    if (main_window_) {
        main_window_->RefreshWorkspace();
    }
}

void HaikuShellHost::SetWazooWindowVisible(std::string_view group_id, bool visible) {
    for (const auto& window : wazoo_windows_) {
        if (!window || window->GroupId() != group_id) {
            continue;
        }
        if (visible) {
            const auto tools = GroupTools(group_id);
            const std::string tool_id = tools.empty() ? std::string() : tools.front().id;
            if (!tool_id.empty()) {
                window->ActivateTool(tool_id);
            }
        } else {
            window->PostMessage(B_QUIT_REQUESTED);
        }
        return;
    }

    if (!visible) {
        return;
    }

    const auto tools = GroupTools(group_id);
    if (!tools.empty()) {
        OpenToolWindow(tools.front().id);
    }
}

void HaikuShellHost::ShowMainWindow() {
    if (!main_window_) {
        main_window_ = std::make_unique<HaikuMainWindow>(*this);
    }

    if (main_window_->IsHidden()) {
        main_window_->Show();
    } else {
        main_window_->Activate(true);
    }
    RestoreWazooWindows();

    if (pending_composer_message_) {
        ShowComposeWindow(*pending_composer_message_);
        pending_composer_message_.reset();
    }
}

void HaikuShellHost::ShowComposeWindow(const ComposeMessage& message) {
    auto compose_window = std::make_unique<HaikuComposeWindow>(*this, message);
    compose_window->Show();
    compose_windows_.push_back(std::move(compose_window));
}

void HaikuShellHost::ReloadWorkspace() {
    workspace_ = std::make_unique<InMemoryWorkspaceModel>();

    for (const auto& mailbox : mailbox_store_->ListMailboxes()) {
        const bool is_drafts_mailbox = mailbox.id == "drafts";
        const auto stored_messages = is_drafts_mailbox ? std::vector<MessageRecord>{}
                                                       : message_store_->ListMessages(mailbox.id);
        const bool show_deleted =
            mailbox.protocol != MailboxProtocol::kImap || MailboxShowsDeleted(mailbox.id);
        std::vector<MessageRecord> messages;
        messages.reserve(stored_messages.size());
        for (const auto& message : stored_messages) {
            if (!show_deleted && message.deleted) {
                continue;
            }
            messages.push_back(message);
        }
        std::size_t unread_count = 0;
        for (const auto& message : messages) {
            if (message.unread) {
                ++unread_count;
            }
        }

        workspace_->AddMailbox({mailbox.id,
                                mailbox.display_name,
                                unread_count,
                                MailboxParentId(mailbox),
                                mailbox.account_id,
                                MailboxProtocolLabel(mailbox.protocol),
                                mailbox.system_mailbox,
                                mailbox.is_remote});
        for (const auto& message : messages) {
            const std::string preview = PreviewText(message.plain_text_body);
            workspace_->AddMessage({
                message.id,
                mailbox.id,
                message.subject,
                message.sender,
                preview,
                message.unread,
                message.attachments.size(),
                LegacyMessageStatusLabel(message.legacy_status),
                PriorityLabel(message.compose_options.priority),
                message.legacy_status,
                message.label_index,
                message.junk_score,
                message.manually_junked,
                message.pop_server_status,
                message.attachments_omitted,
                message.download_complete,
                message.plain_text_body.size() + message.html_body.size(),
                message.updated_at != 0 ? message.updated_at : message.created_at,
            });
            MessageDetail detail;
            detail.id = message.id;
            detail.mailbox_id = mailbox.id;
            detail.subject = message.subject;
            detail.sender = message.sender;
            detail.recipients = message.recipients;
            detail.preview = preview;
            detail.plain_text_body = message.plain_text_body;
            detail.html_body = message.html_body;
            detail.rtf_body = message.rtf_body;
            detail.paige_native_body = message.paige_native_body;
            detail.styled_source = message.styled_source;
            detail.styled_fidelity = message.styled_fidelity;
            detail.unread = message.unread;
            detail.download_complete = message.download_complete;
            detail.attachments_omitted = message.attachments_omitted;
            detail.flagged = message.flagged;
            detail.deleted = message.deleted;
            detail.answered = message.answered;
            detail.legacy_status = message.legacy_status;
            detail.label_index = message.label_index;
            detail.junk_score = message.junk_score;
            detail.manually_junked = message.manually_junked;
            detail.pop_server_status = message.pop_server_status;
            detail.last_error = message.last_error;
            for (const auto& attachment : message.attachments) {
                detail.attachments.push_back(BuildAttachmentSummary(attachment));
            }
            workspace_->AddMessageDetail(detail);
        }
    }

    const auto drafts = draft_store_->ListDrafts();
    if (!drafts.empty() && !mailbox_store_->GetMailbox("drafts")) {
        std::string ignored;
        mailbox_store_->EnsureMailbox(
            {"drafts", "Drafts", {}, "", MailboxProtocol::kLocal, "", false, false, drafts.size()}, &ignored);
    }
    for (const auto& draft : drafts) {
        const std::string preview = PreviewText(draft.body.plain_text);
        workspace_->AddMessage({
            draft.id,
            "drafts",
            draft.headers.subject.empty() ? "(No subject)" : draft.headers.subject,
            draft.headers.from_persona,
            preview,
            false,
            draft.attachments.size(),
            "draft",
            PriorityLabel(draft.options.priority),
            LegacyMessageStatus::kUnsent,
            0,
            0,
            false,
            PopServerStatus::kNone,
            false,
            true,
            draft.body.plain_text.size() + draft.body.html_fragment.size(),
            0,
        });
        MessageDetail detail;
        detail.id = draft.id;
        detail.mailbox_id = "drafts";
        detail.subject = draft.headers.subject.empty() ? "(No subject)" : draft.headers.subject;
        detail.sender = draft.headers.from_persona;
        detail.recipients = draft.headers.to;
        detail.preview = preview;
        detail.plain_text_body = draft.body.plain_text;
        detail.html_body = draft.body.html_fragment;
        detail.rtf_body = draft.body.rtf_fragment;
        detail.paige_native_body = draft.body.paige_native_bytes;
        detail.styled_source = draft.body.styled_source;
        detail.styled_fidelity = draft.body.fidelity;
        detail.unread = false;
        detail.download_complete = true;
        detail.attachments_omitted = false;
        detail.legacy_status = LegacyMessageStatus::kUnsent;
        detail.label_index = 0;
        detail.junk_score = 0;
        detail.manually_junked = false;
        detail.pop_server_status = PopServerStatus::kNone;
        for (const auto& attachment : draft.attachments) {
            detail.attachments.push_back(BuildAttachmentSummary(attachment));
        }
        workspace_->AddMessageDetail(detail);
    }

    if (main_window_) {
        main_window_->RefreshWorkspace();
    }
    for (const auto& window : message_windows_) {
        if (window) {
            window->RefreshFromWorkspace();
        }
    }
    for (const auto& window : wazoo_windows_) {
        if (window) {
            window->Refresh();
        }
    }
}

void HaikuShellHost::EnsureWorkspaceDirectories() {
    std::error_code ignored;
    std::filesystem::create_directories(DataRoot(), ignored);
    std::filesystem::create_directories(DataRoot() / "Plugins", ignored);
    std::filesystem::remove_all(DataRoot() / "Cache" / "WebKit" / "messages", ignored);
    std::filesystem::remove_all(DataRoot() / "Cache" / "WebKit" / "compose", ignored);
    std::filesystem::create_directories(DataRoot() / "Cache" / "WebKit", ignored);
    std::filesystem::create_directories(DataRoot() / "help", ignored);
    std::filesystem::create_directories(DataRoot() / "profile_snapshots", ignored);
    std::filesystem::create_directories(DataRoot() / "import", ignored);
    std::filesystem::create_directories(DataRoot() / "Stationery", ignored);
    std::filesystem::create_directories(DataRoot() / "Signatures", ignored);

    std::string error_message;
    mailbox_store_->EnsureMailbox({"inbox", "Inbox", {}, "", MailboxProtocol::kLocal, "", false, true, 0},
                                  &error_message);
    mailbox_store_->EnsureMailbox({"out", "Out", {}, "", MailboxProtocol::kLocal, "", false, true, 0},
                                  &error_message);
    mailbox_store_->EnsureMailbox({"drafts", "Drafts", {}, "", MailboxProtocol::kLocal, "", false, false, 0},
                                  &error_message);
    BootstrapTemplatesIfNeeded();
}

void HaikuShellHost::LoadBootstrapAccounts() {
    const auto bootstrap_profile =
        SourceRoot() / "tests" / "fixtures" / "legacy" / "profile_snapshots" / "Eudora.box";
    const auto active_profile = std::filesystem::exists(SettingsPath()) ? SettingsPath() : bootstrap_profile;

    std::string ignored;
    if (!settings_->LoadFromFile(active_profile, &ignored)) {
        settings_->LoadFromFile(bootstrap_profile, &ignored);
    }
    if (!account_service_->LoadFromSettings(*settings_) && active_profile != bootstrap_profile) {
        settings_->LoadFromFile(bootstrap_profile, &ignored);
        account_service_->LoadFromSettings(*settings_);
    }
}

std::filesystem::path HaikuShellHost::DataRoot() const {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "config" / "settings" /
               std::string(hermes::kHemeraStableDataDirectoryName);
    }
    return std::filesystem::current_path() / "var" / "haiku-shell";
}

std::filesystem::path HaikuShellHost::SettingsPath() const {
    return DataRoot() / "EUDORA.ini";
}

void HaikuShellHost::RememberRecentMailbox(std::string_view mailbox_id) {
    const auto mailbox = mailbox_store_->GetMailbox(mailbox_id);
    if (!mailbox || mailbox->kind != MailboxKind::kMailbox) {
        return;
    }

    auto preferences = GuiPreferencesFromSettings(*settings_);
    const auto recent_mailbox_ids =
        hermes::RememberRecentMailboxId(preferences.recent_mailbox_ids,
                                        *mailbox_store_,
                                        mailbox->id,
                                        MaxRecentMailboxCount(*settings_));

    if (recent_mailbox_ids == preferences.recent_mailbox_ids) {
        return;
    }

    preferences.recent_mailbox_ids = std::move(recent_mailbox_ids);
    ApplyGuiPreferencesToSettings(preferences, *settings_);
    std::string ignored;
    PersistSettings(&ignored);

    for (const auto& window : wazoo_windows_) {
        if (window && window->GroupId() == "mailboxes") {
            window->Refresh();
        }
    }
}

bool HaikuShellHost::PersistSettings(std::string* error_message) {
    std::error_code ignored;
    std::filesystem::create_directories(DataRoot(), ignored);
    return settings_->SaveToFile(SettingsPath(), error_message);
}

void HaikuShellHost::RecordAttachmentLaunch(std::string_view title,
                                            const std::filesystem::path& path,
                                            std::string_view source_context) {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    link_history_store_->AddEntry({"attachment-" + std::to_string(static_cast<long long>(now)),
                                   LinkHistoryKind::kAttachment,
                                   std::string(title),
                                   path.string(),
                                   std::string(source_context),
                                   true,
                                   static_cast<std::int64_t>(now)});
    std::string ignored;
    link_history_store_->SaveToFile(DataRoot() / "LinkHistory.ini", &ignored);
}

bool HaikuShellHost::ShowMessage(std::string_view mailbox_id, std::string_view message_id) {
    if (!main_window_) {
        return false;
    }
    OpenMailbox(mailbox_id);
    main_window_->SetCurrentMailbox(std::string(mailbox_id), false);
    return main_window_->SelectMessage(std::string(message_id), true);
}

bool HaikuShellHost::OpenMessageWindow(std::string_view mailbox_id, std::string_view message_id) {
    for (const auto& window : message_windows_) {
        if (window && window->MatchesMessage(mailbox_id, message_id)) {
            window->LoadMessage(std::string(mailbox_id), std::string(message_id));
            if (window->IsHidden()) {
                window->Show();
            } else {
                window->Activate(true);
            }
            return true;
        }
    }

    auto window = std::make_unique<HaikuMessageWindow>(*this, std::string(mailbox_id), std::string(message_id));
    if (!window->LoadMessage(std::string(mailbox_id), std::string(message_id))) {
        return false;
    }
    window->Show();
    message_windows_.push_back(std::move(window));
    return true;
}

bool HaikuShellHost::OpenToolWindow(std::string_view tool_id) {
    const std::string requested(tool_id);
    const std::string group_id = GroupIdForTool(requested);
    if (group_id.empty()) {
        return false;
    }

    for (const auto& window : wazoo_windows_) {
        if (window && window->GroupId() == group_id) {
            return window->ActivateTool(requested);
        }
    }

    auto preferences = GuiPreferencesFromSettings(*settings_);
    hermes::WazooWindowState initial_state = WazooStateForGroup(preferences, group_id);
    initial_state.open = true;
    auto window = std::make_unique<HaikuWazooWindow>(
        *this, group_id, GroupTitle(group_id), GroupTools(group_id), initial_state);
    const bool activated = window->ActivateTool(requested);
    wazoo_windows_.push_back(std::move(window));
    return activated;
}

bool HaikuShellHost::OpenHelpWindow() {
    if (!help_window_) {
        help_window_ = std::make_unique<HaikuHelpWindow>(*this);
    } else {
        help_window_->RefreshCatalog();
    }

    if (help_window_->IsHidden()) {
        help_window_->Show();
    } else {
        help_window_->Activate(true);
    }
    return true;
}

bool HaikuShellHost::OpenImportWindow() {
    if (!import_window_) {
        import_window_ = std::make_unique<HaikuImportWindow>(*this);
    }

    if (import_window_->IsHidden()) {
        import_window_->Show();
    } else {
        import_window_->Activate(true);
    }
    return true;
}

std::filesystem::path HaikuShellHost::UserPluginRootPath() const {
    return DataRoot() / "Plugins";
}

std::filesystem::path HaikuShellHost::AppPluginRootPath() const {
    if (be_app != nullptr) {
        app_info info;
        if (be_app->GetAppInfo(&info) == B_OK) {
            BEntry entry(&info.ref, true);
            BPath path;
            if (entry.GetPath(&path) == B_OK) {
                return std::filesystem::path(path.Path()).parent_path() / "Plugins";
            }
        }
    }
    return std::filesystem::current_path() / "Plugins";
}

std::vector<std::filesystem::path> HaikuShellHost::PluginDiscoveryRoots() const {
    std::vector<std::filesystem::path> roots;
    roots.push_back(UserPluginRootPath());
    const auto app_root = AppPluginRootPath();
    if (std::find(roots.begin(), roots.end(), app_root) == roots.end()) {
        roots.push_back(app_root);
    }
    return roots;
}

std::vector<std::string> HaikuShellHost::PluginScanErrors() const {
    return plugin_scan_errors_;
}

bool HaikuShellHost::MailboxAutoSyncEnabled(std::string_view mailbox_id) const {
    const auto mailbox = mailbox_store_->GetMailbox(mailbox_id);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap || mailbox->account_id.empty()) {
        return false;
    }
    const auto state =
        sync_state_store_->LoadImapState(mailbox->account_id, mailbox->id)
            .value_or(ImapMailboxSyncState{mailbox->account_id, mailbox->id});
    return state.auto_sync;
}

bool HaikuShellHost::SetMailboxAutoSyncEnabled(std::string_view mailbox_id, bool enabled) {
    const auto mailbox = mailbox_store_->GetMailbox(mailbox_id);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap || mailbox->account_id.empty()) {
        return false;
    }
    ImapMailboxSyncState state =
        sync_state_store_->LoadImapState(mailbox->account_id, mailbox->id)
            .value_or(ImapMailboxSyncState{mailbox->account_id, mailbox->id});
    state.auto_sync = enabled;
    std::string error_message;
    return sync_state_store_->SaveImapState(state, &error_message);
}

bool HaikuShellHost::MailboxShowsDeleted(std::string_view mailbox_id) const {
    const auto mailbox = mailbox_store_->GetMailbox(mailbox_id);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap || mailbox->account_id.empty()) {
        return false;
    }
    const auto state =
        sync_state_store_->LoadImapState(mailbox->account_id, mailbox->id)
            .value_or(ImapMailboxSyncState{mailbox->account_id, mailbox->id});
    return state.show_deleted;
}

bool HaikuShellHost::SetMailboxShowsDeleted(std::string_view mailbox_id, bool show_deleted) {
    const auto mailbox = mailbox_store_->GetMailbox(mailbox_id);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap || mailbox->account_id.empty()) {
        return false;
    }
    ImapMailboxSyncState state =
        sync_state_store_->LoadImapState(mailbox->account_id, mailbox->id)
            .value_or(ImapMailboxSyncState{mailbox->account_id, mailbox->id});
    state.show_deleted = show_deleted;
    std::string error_message;
    if (!sync_state_store_->SaveImapState(state, &error_message)) {
        return false;
    }
    ReloadWorkspace();
    return true;
}

bool HaikuShellHost::ResyncMailboxTree(std::string_view mailbox_id) {
    if (ShellBehavior().offline) {
        return false;
    }
    const auto mailbox = mailbox_store_->GetMailbox(mailbox_id);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap) {
        return false;
    }

    const auto mailboxes = mailbox_store_->ListMailboxes();
    std::map<std::string, std::string> parent_by_id;
    for (const auto& entry : mailboxes) {
        parent_by_id.emplace(entry.id, entry.parent_id);
    }

    bool queued_any = false;
    for (const auto& entry : mailboxes) {
        if (entry.protocol != MailboxProtocol::kImap) {
            continue;
        }
        if (entry.id == mailbox->id) {
            queued_any = ResyncMailbox(entry.id) || queued_any;
            continue;
        }
        auto current = parent_by_id.find(entry.id);
        while (current != parent_by_id.end() && !current->second.empty()) {
            if (current->second == mailbox->id) {
                queued_any = ResyncMailbox(entry.id) || queued_any;
                break;
            }
            current = parent_by_id.find(current->second);
        }
    }
    return queued_any;
}

MailboxUiSettings HaikuShellHost::MailboxUi() const {
    return MailboxUiSettingsFromSettings(*settings_);
}

ShellBehaviorSettings HaikuShellHost::ShellBehavior() const {
    return ShellBehaviorSettingsFromSettings(*settings_);
}

MailTransferSettings HaikuShellHost::MailTransfer() const {
    return MailTransferSettingsFromSettings(*settings_);
}

MailTransportSummary HaikuShellHost::ExecuteMailTransfer(const MailTransferRequest& request) {
    MailTransportSummary summary;
    if (ShellBehavior().offline) {
        summary.error_message = "Work Offline is enabled.";
        return summary;
    }
    summary = transport_coordinator_->ExecuteMailTransfer(request);
    ApplyPendingFilters();
    ReloadWorkspace();
    return summary;
}

bool HaikuShellHost::SetOfflineMode(bool offline) {
    auto behavior = ShellBehavior();
    if (behavior.offline == offline) {
        return true;
    }
    behavior.offline = offline;
    ApplyShellBehaviorSettingsToSettings(behavior, *settings_);
    std::string error_message;
    if (!PersistSettings(&error_message)) {
        return false;
    }
    if (main_window_) {
        main_window_->RefreshWorkspace();
    }
    return true;
}

bool HaikuShellHost::ClearMailboxContents(std::string_view mailbox_id, std::string* error_message) {
    const auto mailbox = mailbox_store_->GetMailbox(mailbox_id);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kLocal || mailbox->id == "out" || mailbox->id == "drafts") {
        if (error_message != nullptr) {
            *error_message = "Only local non-Out, non-Drafts mailboxes can be cleared directly.";
        }
        return false;
    }

    bool removed_any = false;
    for (const auto& message : message_store_->ListMessages(mailbox_id)) {
        if (!message_store_->DeleteMessage(mailbox_id, message.id, error_message)) {
            return false;
        }
        removed_any = true;
    }
    if (removed_any) {
        ReloadWorkspace();
    }
    return true;
}

bool HaikuShellHost::CompactMailboxes(std::string* error_message) {
    if (!hermes::eudora::CompactAllMailboxes(DataRoot(), error_message)) {
        return false;
    }
    ReloadWorkspace();
    return true;
}

bool HaikuShellHost::ForgetAllCredentialsAndTokens(std::string* error_message) {
    if (!credential_store_->ClearAllCredentials(error_message)) {
        return false;
    }
    for (const auto& account : account_service_->Accounts()) {
        std::string token_error;
        if (!oauth_token_store_->DeleteToken(account.id, &token_error) && !token_error.empty()) {
            if (error_message != nullptr) {
                *error_message = token_error;
            }
            return false;
        }
    }
    return true;
}

bool HaikuShellHost::UpdateAccountPasswords(std::string_view account_id,
                                            std::string_view incoming_password,
                                            std::string_view outgoing_password,
                                            std::string* error_message) {
    const auto account = account_service_->FindById(account_id);
    if (!account) {
        if (error_message != nullptr) {
            *error_message = "Unknown account.";
        }
        return false;
    }

    if (!incoming_password.empty() &&
        !credential_store_->SaveCredential(account_id,
                                           CredentialKind::kIncoming,
                                           incoming_password,
                                           error_message)) {
        return false;
    }
    if (!outgoing_password.empty() &&
        !credential_store_->SaveCredential(account_id,
                                           CredentialKind::kOutgoing,
                                           outgoing_password,
                                           error_message)) {
        return false;
    }
    return true;
}

bool HaikuShellHost::SendActiveWindowToBack() {
    std::vector<BWindow*> visible_windows;
    const auto add_window = [&visible_windows](BWindow* window) {
        if (window != nullptr && !window->IsHidden()) {
            visible_windows.push_back(window);
        }
    };

    add_window(main_window_.get());
    for (const auto& window : wazoo_windows_) {
        add_window(window.get());
    }
    for (const auto& window : compose_windows_) {
        add_window(window.get());
    }
    for (const auto& window : message_windows_) {
        add_window(window.get());
    }
    add_window(help_window_.get());
    add_window(import_window_.get());

    auto* active = static_cast<BWindow*>(nullptr);
    for (auto* window : visible_windows) {
        if (window->IsActive()) {
            active = window;
            break;
        }
    }
    if (active == nullptr) {
        active = main_window_.get();
    }
    return active != nullptr && active->SendBehind(nullptr) == B_OK;
}

bool HaikuShellHost::ArrangeManagedWindows(WindowArrangeMode mode) {
    std::vector<BWindow*> windows;
    const auto add_window = [&windows](BWindow* window) {
        if (window != nullptr && !window->IsHidden()) {
            windows.push_back(window);
        }
    };

    add_window(main_window_.get());
    for (const auto& window : wazoo_windows_) {
        add_window(window.get());
    }
    for (const auto& window : compose_windows_) {
        add_window(window.get());
    }
    for (const auto& window : message_windows_) {
        add_window(window.get());
    }
    add_window(help_window_.get());
    add_window(import_window_.get());

    if (windows.empty()) {
        return false;
    }

    BScreen screen;
    BRect available = screen.Frame();
    available.InsetBy(24.0f, 48.0f);

    switch (mode) {
        case WindowArrangeMode::kCascade:
        case WindowArrangeMode::kArrange: {
            const float width = std::max(420.0f, available.Width() * 0.72f);
            const float height = std::max(320.0f, available.Height() * 0.72f);
            const float x_limit = std::max(available.left, available.right - width);
            const float y_limit = std::max(available.top, available.bottom - height);
            const float step = 26.0f;
            for (std::size_t index = 0; index < windows.size(); ++index) {
                const float x = std::min(x_limit, available.left + step * static_cast<float>(index));
                const float y = std::min(y_limit, available.top + step * static_cast<float>(index));
                windows[index]->MoveTo(x, y);
                windows[index]->ResizeTo(width, height);
            }
            return true;
        }
        case WindowArrangeMode::kTileHorizontally: {
            const float row_height = available.Height() / std::max<std::size_t>(1, windows.size());
            for (std::size_t index = 0; index < windows.size(); ++index) {
                const float top = available.top + row_height * static_cast<float>(index);
                const float height = std::max(180.0f, row_height - 8.0f);
                windows[index]->MoveTo(available.left, top);
                windows[index]->ResizeTo(available.Width(), height);
            }
            return true;
        }
        case WindowArrangeMode::kTileVertically: {
            const float column_width = available.Width() / std::max<std::size_t>(1, windows.size());
            for (std::size_t index = 0; index < windows.size(); ++index) {
                const float left = available.left + column_width * static_cast<float>(index);
                const float width = std::max(260.0f, column_width - 8.0f);
                windows[index]->MoveTo(left, available.top);
                windows[index]->ResizeTo(width, available.Height());
            }
            return true;
        }
    }

    return false;
}

bool HaikuShellHost::CloseAllManagedWindows() {
    bool hid_any = false;
    const auto hide_window = [&hid_any](BWindow* window) {
        if (window != nullptr && !window->IsHidden()) {
            window->Hide();
            hid_any = true;
        }
    };

    for (const auto& window : wazoo_windows_) {
        hide_window(window.get());
    }
    for (const auto& window : compose_windows_) {
        hide_window(window.get());
    }
    for (const auto& window : message_windows_) {
        hide_window(window.get());
    }
    hide_window(help_window_.get());
    hide_window(import_window_.get());
    return hid_any;
}

bool HaikuShellHost::SaveOpenWindowLayout(std::string* error_message) {
    if (main_window_ != nullptr) {
        main_window_->SaveWindowLayoutState();
    }

    auto preferences = GuiPreferencesFromSettings(*settings_);

    for (const auto& window : wazoo_windows_) {
        if (window == nullptr) {
            continue;
        }
        const auto state = window->CurrentState();
        if (window->GroupId() == "mailboxes") {
            preferences.mailboxes_wazoo = state;
        } else if (window->GroupId() == "tools") {
            preferences.tools_wazoo = state;
        } else if (window->GroupId() == "tasks") {
            preferences.tasks_wazoo = state;
        }
    }

    ApplyGuiPreferencesToSettings(preferences, *settings_);
    if (!PersistSettings(error_message)) {
        return false;
    }

    if (main_window_ != nullptr) {
        main_window_->RefreshWorkspace();
    }
    return true;
}

std::optional<MailboxRecord> HaikuShellHost::FindMailboxRole(std::string_view account_id,
                                                             std::string_view role_name) const {
    const std::string role = ToLower(std::string(role_name));
    for (const auto& mailbox : mailbox_store_->ListMailboxes()) {
        if (!account_id.empty() && mailbox.account_id != account_id) {
            continue;
        }
        const std::string id = ToLower(mailbox.id);
        const std::string display_name = ToLower(mailbox.display_name);
        const std::string remote_name = ToLower(mailbox.remote_name);
        if (role == "inbox") {
            if (id == "inbox" || display_name == "inbox" || display_name == "in" || remote_name == "inbox") {
                return mailbox;
            }
            continue;
        }
        if (id == role || display_name == role || remote_name == role ||
            display_name.find(role) != std::string::npos || remote_name.find(role) != std::string::npos) {
            return mailbox;
        }
    }
    return std::nullopt;
}

bool HaikuShellHost::SetLegacyStatusForMessages(std::string_view mailbox_id,
                                                const std::vector<std::string>& message_ids,
                                                LegacyMessageStatus status) {
    std::string error_message;
    const bool success = hermes::SetLegacyStatus(*message_store_, mailbox_id, message_ids, status, &error_message);
    if (success) {
        ReloadWorkspace();
    }
    return success;
}

bool HaikuShellHost::SetLabelForMessages(std::string_view mailbox_id,
                                         const std::vector<std::string>& message_ids,
                                         int label_index) {
    std::string error_message;
    const bool success = hermes::SetLabel(*message_store_, mailbox_id, message_ids, label_index, &error_message);
    if (success) {
        ReloadWorkspace();
    }
    return success;
}

bool HaikuShellHost::SetPopServerStatusForMessages(std::string_view mailbox_id,
                                                   const std::vector<std::string>& message_ids,
                                                   PopServerStatus status) {
    const auto mailbox = mailbox_store_->GetMailbox(mailbox_id);
    if (!mailbox) {
        return false;
    }

    const PopServerStatus effective_status =
        hermes::EffectivePopServerStatus(*mailbox, status, MailboxUi().delete_fetched_junk);
    for (const auto& message_id : message_ids) {
        const auto message = message_store_->GetMessage(mailbox_id, message_id);
        if (!message || message->remote_id.empty()) {
            return false;
        }
        const auto account = account_service_->FindById(message->account_id);
        if (!account || !account->uses_pop) {
            return false;
        }
    }

    std::string error_message;
    if (!hermes::SetPopServerStatus(*message_store_,
                                    *mailbox,
                                    message_ids,
                                    effective_status,
                                    MailboxUi().delete_fetched_junk,
                                    &error_message)) {
        return false;
    }

    std::map<std::string, PopSyncState> states_by_account;
    for (const auto& message_id : message_ids) {
        const auto message = message_store_->GetMessage(mailbox_id, message_id);
        if (!message) {
            return false;
        }
        auto& state = states_by_account[message->account_id];
        if (state.account_id.empty()) {
            state = sync_state_store_->LoadPopState(message->account_id).value_or(
                PopSyncState{message->account_id, {}, {}});
        }
        state.uidl_to_message_id[message->remote_id] = message->id;
        if (effective_status == PopServerStatus::kNone) {
            state.uidl_to_server_status.erase(message->remote_id);
        } else {
            state.uidl_to_server_status[message->remote_id] = effective_status;
        }
    }

    for (const auto& entry : states_by_account) {
        if (!sync_state_store_->SavePopState(entry.second, &error_message)) {
            return false;
        }
    }

    ReloadWorkspace();
    return true;
}

bool HaikuShellHost::ChangeQueueingForMessages(std::string_view mailbox_id,
                                               const std::vector<std::string>& message_ids,
                                               int delay_seconds) {
    if (mailbox_id != "out") {
        return false;
    }
    const std::int64_t scheduled_send_at =
        delay_seconds > 0 ? static_cast<std::int64_t>(std::time(nullptr)) + delay_seconds : 0;
    bool updated_any = false;
    std::string error_message;
    for (const auto& message_id : message_ids) {
        updated_any = transport_coordinator_->UpdateQueuedMessageTiming(
                          mailbox_id, message_id, scheduled_send_at, &error_message) ||
                      updated_any;
    }
    if (updated_any) {
        ReloadWorkspace();
    }
    return updated_any;
}

bool HaikuShellHost::ApplyFiltersToMessages(std::string_view mailbox_id,
                                            const std::vector<std::string>& message_ids) {
    const auto mailbox = mailbox_store_->GetMailbox(mailbox_id);
    if (!mailbox) {
        return false;
    }
    std::string error_message;
    const bool success =
        hermes::ApplyFiltersToMessages(*message_store_, *mailbox_store_, *filter_report_store_, *mailbox, message_ids,
                                       *filter_store_, &error_message);
    if (success) {
        filter_report_store_->SaveToFile(DataRoot() / "FilterReport.ini", nullptr);
        ReloadWorkspace();
    }
    return success;
}

std::optional<FilterRule> HaikuShellHost::CreateManualFilterFromMessages(std::string_view mailbox_id,
                                                                         const std::vector<std::string>& message_ids,
                                                                         std::string* error_message) {
    std::vector<MessageRecord> messages;
    messages.reserve(message_ids.size());
    for (const auto& message_id : message_ids) {
        const auto message = message_store_->GetMessage(mailbox_id, message_id);
        if (!message) {
            if (error_message) {
                *error_message = "Unable to locate message " + message_id + ".";
            }
            return std::nullopt;
        }
        messages.push_back(*message);
    }

    const auto suggestion = hermes::SuggestManualFilter(messages);
    if (!suggestion) {
        if (error_message) {
            *error_message = "Unable to derive a common filter from the selected messages.";
        }
        return std::nullopt;
    }

    filter_store_->AddOrReplace(suggestion->rule);
    if (!filter_store_->SaveToFile(DataRoot() / "Filters.ini", error_message)) {
        return std::nullopt;
    }
    ReloadWorkspace();
    return suggestion->rule;
}

bool HaikuShellHost::ApplyJunkActionToMessages(std::string_view mailbox_id,
                                               const std::vector<std::string>& message_ids,
                                               MailboxJunkAction action) {
    const auto mailbox = mailbox_store_->GetMailbox(mailbox_id);
    if (!mailbox) {
        return false;
    }

    HeuristicJunkScorer junk_scorer(address_book_service_.get());
    std::string error_message;
    if (mailbox->protocol != MailboxProtocol::kImap) {
        const bool success = hermes::ApplyJunkActionToLocalMessages(*message_store_,
                                                                    *mailbox_store_,
                                                                    *filter_report_store_,
                                                                    *mailbox,
                                                                    message_ids,
                                                                    action,
                                                                    junk_scorer,
                                                                    *filter_store_,
                                                                    &error_message);
        if (success) {
            filter_report_store_->SaveToFile(DataRoot() / "FilterReport.ini", nullptr);
            ReloadWorkspace();
        }
        return success;
    }

    bool success = true;
    for (const auto& message_id : message_ids) {
        auto record = message_store_->GetMessage(mailbox_id, message_id);
        if (!record) {
            success = false;
            continue;
        }

        std::optional<MailboxRecord> destination;
        if (action == MailboxJunkAction::kJunk) {
            record->manually_junked = true;
            record->junk_score = 100;
            destination = FindMailboxRole(mailbox->account_id, "junk");
        } else if (action == MailboxJunkAction::kNotJunk) {
            record->manually_junked = false;
            record->junk_score = 0;
            destination = FindMailboxRole(mailbox->account_id, "inbox");
        } else {
            const auto verdict = junk_scorer.Score(*record);
            record->manually_junked = false;
            record->junk_score = std::clamp(verdict.score, 0, 127);
            if (verdict.is_junk) {
                destination = FindMailboxRole(mailbox->account_id, "junk");
            } else if (ToLower(mailbox->display_name) == "junk" || ToLower(mailbox->remote_name) == "junk") {
                destination = FindMailboxRole(mailbox->account_id, "inbox");
            }
        }

        record->updated_at = std::time(nullptr);
        if (!message_store_->SaveMessage(*record, &error_message)) {
            success = false;
            continue;
        }

        if (destination && destination->id != mailbox->id) {
            if (!transport_coordinator_->QueueMoveMessage(mailbox->id, message_id, destination->id, &error_message)) {
                success = false;
            }
        }
    }

    if (success) {
        ReloadWorkspace();
    }
    return success;
}

bool HaikuShellHost::RescanPlugins(std::string* error_message) {
    plugin_scan_errors_.clear();
    std::error_code create_error;
    std::filesystem::create_directories(UserPluginRootPath(), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create plugin root: " + create_error.message();
        }
        return false;
    }

    struct CandidatePlugin {
        PluginSummary summary;
        std::filesystem::path source_root;
        int priority = 0;
    };

    std::map<std::string, CandidatePlugin> selected;
    const auto roots = PluginDiscoveryRoots();
    for (const auto& root : roots) {
        if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
            continue;
        }

        const int priority = root == UserPluginRootPath() ? 0 : 1;
        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            if (!entry.is_regular_file() || !HasPluginExtension(entry.path())) {
                continue;
            }

            FilesystemPluginHost probe_host;
            std::string load_error;
            if (!probe_host.LoadPlugin(entry.path(), &load_error) || probe_host.Plugins().empty()) {
                if (!load_error.empty()) {
                    plugin_scan_errors_.push_back(entry.path().string() + ": " + load_error);
                }
                continue;
            }

            CandidatePlugin candidate{probe_host.Plugins().front(), root, priority};
            const std::string key = candidate.summary.identifier.empty()
                                        ? candidate.summary.path.filename().string()
                                        : candidate.summary.identifier;
            const auto existing = selected.find(key);
            if (existing == selected.end() || candidate.priority < existing->second.priority) {
                selected[key] = candidate;
            }
        }
    }

    plugin_host_ = std::make_unique<FilesystemPluginHost>();
    for (const auto& [key, candidate] : selected) {
        std::string load_error;
        if (!plugin_host_->LoadPlugin(candidate.summary.path, &load_error) && !load_error.empty()) {
            plugin_scan_errors_.push_back(candidate.summary.path.string() + ": " + load_error);
        }
    }

    if (error_message && !plugin_scan_errors_.empty()) {
        *error_message = plugin_scan_errors_.front();
    }
    return true;
}

bool HaikuShellHost::ReloadHelpCatalog(std::string* error_message) {
    help_catalog_ = std::make_unique<LegacyHelpCatalog>();

    const auto imported_help_root = DataRoot() / "help";
    const auto fallback_help_root = SourceRoot() / "tests" / "fixtures" / "legacy" / "help";
    const auto help_root = std::filesystem::exists(imported_help_root) &&
                                   std::filesystem::is_directory(imported_help_root) &&
                                   !std::filesystem::is_empty(imported_help_root)
                               ? imported_help_root
                               : fallback_help_root;

    bool loaded_any = false;
    if (std::filesystem::exists(help_root) && std::filesystem::is_directory(help_root)) {
        std::vector<std::filesystem::path> help_files;
        for (const auto& entry : std::filesystem::directory_iterator(help_root)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto extension = entry.path().extension().string();
            if (extension == ".hh" || extension == ".cnt") {
                help_files.push_back(entry.path());
            }
        }
        std::sort(help_files.begin(), help_files.end());
        for (const auto& path : help_files) {
            std::string load_error;
            bool loaded = false;
            if (path.extension() == ".hh") {
                loaded = help_catalog_->LoadTopicMap(path, &load_error);
            } else if (path.extension() == ".cnt") {
                loaded = help_catalog_->LoadContents(path, &load_error);
            }
            if (loaded) {
                loaded_any = true;
            } else if (!load_error.empty() && error_message != nullptr && error_message->empty()) {
                *error_message = load_error;
            }
        }
    }

    if (help_window_ != nullptr) {
        help_window_->RefreshCatalog();
    }

    if (!loaded_any && error_message != nullptr && error_message->empty()) {
        *error_message = "No help catalog artifacts were loaded.";
    }
    return loaded_any;
}

bool HaikuShellHost::RevealHelpFiles() {
    const auto imported_help_root = DataRoot() / "help";
    const auto fallback_help_root = SourceRoot() / "tests" / "fixtures" / "legacy" / "help";
    const auto help_root = std::filesystem::exists(imported_help_root) &&
                                   std::filesystem::is_directory(imported_help_root) &&
                                   !std::filesystem::is_empty(imported_help_root)
                               ? imported_help_root
                               : fallback_help_root;
    return LaunchPath(help_root);
}

bool HaikuShellHost::ReloadImportedShellState(std::string_view settings_snapshot_name,
                                              std::string* error_message) {
    if (!settings_snapshot_name.empty()) {
        const auto source = DataRoot() / "profile_snapshots" / std::string(settings_snapshot_name);
        if (!std::filesystem::exists(source)) {
            if (error_message) {
                *error_message = "Imported settings snapshot is missing: " + source.string();
            }
            return false;
        }
        std::error_code copy_error;
        std::filesystem::copy_file(source,
                                   SettingsPath(),
                                   std::filesystem::copy_options::overwrite_existing,
                                   copy_error);
        if (copy_error) {
            if (error_message) {
                *error_message = "Unable to apply imported settings snapshot: " + copy_error.message();
            }
            return false;
        }
    }

    LoadBootstrapAccounts();
    LoadToolData();
    ApplyPendingFilters();
    RescanPlugins(nullptr);
    ReloadHelpCatalog(nullptr);
    ReloadWorkspace();
    if (main_window_) {
        main_window_->RefreshWorkspace();
    }
    for (const auto& window : wazoo_windows_) {
        if (window) {
            window->Refresh();
        }
    }
    return true;
}

void HaikuShellHost::RestoreWazooWindows() {
    const auto preferences = GuiPreferencesFromSettings(*settings_);
    for (std::string_view group_id : {"mailboxes", "tools", "tasks"}) {
        const auto& state = WazooStateForGroup(preferences, group_id);
        if (!state.restore_on_launch || !state.open) {
            continue;
        }
        auto tools = GroupTools(group_id);
        if (tools.empty()) {
            continue;
        }
        std::string requested = tools[0].id;
        if (state.selected_tab >= 0 &&
            state.selected_tab < static_cast<int>(tools.size())) {
            requested = tools[static_cast<std::size_t>(state.selected_tab)].id;
        }
        OpenToolWindow(requested);
    }
}

void HaikuShellHost::LoadToolData() {
    std::string ignored;
    nickname_store_->LoadFromFile(DataRoot() / "Nicknames.txt", &ignored);
    filter_store_->LoadFromFile(DataRoot() / "Filters.ini", &ignored);
    filter_report_store_->LoadFromFile(DataRoot() / "FilterReport.ini", &ignored);
    link_history_store_->LoadFromFile(DataRoot() / "LinkHistory.ini", &ignored);
}

void HaikuShellHost::BootstrapTemplatesIfNeeded() {
    const auto fixture_stationery_root =
        SourceRoot() / "tests" / "fixtures" / "legacy" / "compose" / "stationery";
    const auto fixture_signature_root =
        SourceRoot() / "tests" / "fixtures" / "legacy" / "compose" / "signatures";
    const auto live_stationery_root = DataRoot() / "Stationery";
    const auto live_signature_root = DataRoot() / "Signatures";

    stationery_store_->SetRootDirectory(live_stationery_root);
    signature_store_->SetRootDirectory(live_signature_root);

    std::error_code ignored;
    std::filesystem::create_directories(live_stationery_root, ignored);
    std::filesystem::create_directories(live_signature_root, ignored);

    if (std::filesystem::is_empty(live_stationery_root)) {
        FilesystemStationeryStore fixture_store;
        if (fixture_store.Discover(fixture_stationery_root, nullptr)) {
            for (const auto& entry : fixture_store.Templates()) {
                stationery_store_->SaveTemplate(entry, nullptr);
            }
        }
    }

    if (std::filesystem::is_empty(live_signature_root)) {
        FilesystemSignatureStore fixture_store;
        if (fixture_store.Discover(fixture_signature_root, nullptr)) {
            for (const auto& entry : fixture_store.Templates()) {
                signature_store_->SaveTemplate(entry, nullptr);
            }
        }
    }

    stationery_store_->Discover(live_stationery_root, nullptr);
    signature_store_->Discover(live_signature_root, nullptr);
}

void HaikuShellHost::ApplyPendingFilters() {
    if (filter_store_->Rules().empty()) {
        return;
    }

    RuleBasedFilterEngine engine;
    engine.SetRules(filter_store_->Rules());
    std::string ignored;

    for (const auto& mailbox : mailbox_store_->ListMailboxes()) {
        for (auto message : message_store_->ListMessages(mailbox.id)) {
            if (message.delivery_state != MessageDeliveryState::kReceived || message.filters_applied) {
                continue;
            }

            const auto result = engine.Evaluate(message);
            message.filters_applied = true;
            if (result.mark_as_read) {
                message.unread = false;
                message.legacy_status = LegacyMessageStatus::kRead;
            }

            std::string destination_mailbox = mailbox.id;
            if (result.mark_as_junk) {
                message.manually_junked = true;
                message.junk_score = 100;
                destination_mailbox = "junk";
                mailbox_store_->EnsureMailbox({"junk",
                                               "Junk",
                                               {},
                                               message.account_id,
                                               MailboxProtocol::kLocal,
                                               "",
                                               false,
                                               true,
                                               0},
                                              &ignored);
            }
            if (result.destination_mailbox) {
                destination_mailbox = *result.destination_mailbox;
                mailbox_store_->EnsureMailbox({destination_mailbox,
                                               destination_mailbox,
                                               {},
                                               message.account_id,
                                               MailboxProtocol::kLocal,
                                               "",
                                               false,
                                               false,
                                               0},
                                              &ignored);
            }

            message_store_->SaveMessage(message, &ignored);
            if (destination_mailbox != mailbox.id) {
                message_store_->MoveMessage(mailbox.id, message.id, destination_mailbox, &ignored);
            }

            if (result.matched) {
                const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count();
                filter_report_store_->AddEntry({"filter-" + message.id,
                                                message.id,
                                                mailbox.id,
                                                mailbox.display_name,
                                                message.sender,
                                                message.subject,
                                                result.matched_rules,
                                                static_cast<std::int64_t>(now)});
            }
        }
    }

    filter_report_store_->SaveToFile(DataRoot() / "FilterReport.ini", nullptr);
}

}  // namespace hemera::haiku
