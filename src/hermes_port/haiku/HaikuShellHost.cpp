#include "HaikuShellHost.h"

#include <Application.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <system_error>

#include "HaikuComposeWindow.h"
#include "HaikuMainWindow.h"
#include "HaikuToolWindow.h"
#include "hermes/FilterEngine.h"

namespace hermes::haiku_port {

namespace {

std::filesystem::path SourceRoot() {
#ifdef HERMES_SOURCE_ROOT
    return std::filesystem::path(HERMES_SOURCE_ROOT);
#else
    return std::filesystem::current_path();
#endif
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
    return summary;
}

class HermesApplication final : public BApplication {
public:
    explicit HermesApplication(HaikuShellHost& shell_host)
        : BApplication("application/x-vnd.hermes-hemera"),
          shell_host_(shell_host) {}

    void ReadyToRun() override {
        shell_host_.ShowMainWindow();
    }

private:
    HaikuShellHost& shell_host_;
};

}  // namespace

HaikuShellHost::HaikuShellHost()
    : settings_(std::make_unique<IniSettingsStore>()),
      workspace_(std::make_unique<InMemoryWorkspaceModel>()),
      account_service_(std::make_unique<LegacyAccountService>()),
      credential_store_(std::make_unique<FilesystemCredentialStore>(DataRoot())),
      sync_state_store_(std::make_unique<FilesystemSyncStateStore>(DataRoot())),
      imap_action_store_(std::make_unique<FilesystemImapActionStore>(DataRoot())),
      task_model_(std::make_unique<InMemoryMailTaskModel>()),
      draft_store_(std::make_unique<FilesystemDraftStore>(DataRoot() / "drafts")),
      mailbox_store_(std::make_unique<FilesystemMailboxStore>(DataRoot())),
      message_store_(std::make_unique<FilesystemMessageStore>(DataRoot())),
      nickname_store_(std::make_unique<FlatFileNicknameStore>()),
      stationery_store_(std::make_unique<FilesystemStationeryStore>()),
      signature_store_(std::make_unique<FilesystemSignatureStore>()),
      address_book_service_(std::make_unique<MemoryAddressBookService>()),
      filter_store_(std::make_unique<FilesystemFilterStore>()),
      filter_report_store_(std::make_unique<FilesystemFilterReportStore>()),
      link_history_store_(std::make_unique<FilesystemLinkHistoryStore>()),
      tls_provider_(std::make_unique<OpenSslTlsProvider>()),
      transport_service_(std::make_unique<SocketTransportService>(tls_provider_.get())),
      transport_coordinator_(std::make_unique<MailTransportCoordinator>(*account_service_,
                                                                        *credential_store_,
                                                                        *sync_state_store_,
                                                                        *mailbox_store_,
                                                                        *message_store_,
                                                                        *transport_service_,
                                                                        *tls_provider_,
                                                                        *task_model_,
                                                                        imap_action_store_.get())),
      paige_runtime_(std::make_unique<PaigeRuntime>()),
      directory_services_(std::make_unique<LocalDirectoryServiceCatalog>(nickname_store_.get(),
                                                                         address_book_service_.get())) {
    std::string ignored;
    (void)paige_runtime_->Initialize(&ignored);
    EnsureWorkspaceDirectories();
    LoadBootstrapAccounts();
    LoadToolData();
    ApplyPendingFilters();
    ReloadWorkspace();
}

int HaikuShellHost::Run() {
    HermesApplication app(*this);
    app.Run();
    return 0;
}

bool HaikuShellHost::OpenMailbox(std::string_view mailbox_id) {
    active_mailbox_id_ = std::string(mailbox_id);
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
    const auto summary = transport_coordinator_->SendQueued();
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::CheckMail() {
    const auto summary = transport_coordinator_->CheckMail();
    ApplyPendingFilters();
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::SendAndReceive() {
    const auto summary = transport_coordinator_->SendAndReceive();
    ApplyPendingFilters();
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::RefreshMailbox(std::string_view mailbox_id) {
    const auto summary = transport_coordinator_->RefreshMailbox(mailbox_id, false);
    ApplyPendingFilters();
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::ResyncMailbox(std::string_view mailbox_id) {
    const auto summary = transport_coordinator_->RefreshMailbox(mailbox_id, true);
    ApplyPendingFilters();
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::DeleteMessage(std::string_view mailbox_id, std::string_view message_id) {
    std::string error_message;
    const bool queued = transport_coordinator_->QueueDeleteMessage(mailbox_id, message_id, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::UndeleteMessage(std::string_view mailbox_id, std::string_view message_id) {
    std::string error_message;
    const bool queued = transport_coordinator_->QueueUndeleteMessage(mailbox_id, message_id, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::MoveMessage(std::string_view mailbox_id,
                                 std::string_view message_id,
                                 std::string_view destination_mailbox_id) {
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
    std::string error_message;
    const bool queued = transport_coordinator_->QueueCopyMessage(mailbox_id,
                                                                 message_id,
                                                                 destination_mailbox_id,
                                                                 &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::CreateRemoteMailbox(std::string_view account_id, std::string_view remote_name) {
    std::string error_message;
    const bool queued = transport_coordinator_->QueueCreateMailbox(account_id, remote_name, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::RenameRemoteMailbox(std::string_view mailbox_id, std::string_view new_remote_name) {
    std::string error_message;
    const bool queued = transport_coordinator_->QueueRenameMailbox(mailbox_id, new_remote_name, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::DeleteRemoteMailbox(std::string_view mailbox_id) {
    std::string error_message;
    const bool queued = transport_coordinator_->QueueDeleteMailbox(mailbox_id, &error_message);
    ReloadWorkspace();
    return queued;
}

std::optional<std::filesystem::path> HaikuShellHost::AttachmentPath(std::string_view mailbox_id,
                                                                    std::string_view message_id,
                                                                    std::size_t attachment_index) const {
    return message_store_->AttachmentPath(mailbox_id, message_id, attachment_index);
}

bool HaikuShellHost::SaveAttachment(std::string_view mailbox_id,
                                    std::string_view message_id,
                                    std::size_t attachment_index,
                                    const std::filesystem::path& destination_path) {
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
    std::string error_message;
    const bool queued =
        transport_coordinator_->QueueFetchAttachment(mailbox_id, message_id, attachment_index, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::FetchFullMessage(std::string_view mailbox_id, std::string_view message_id) {
    std::string error_message;
    const bool queued =
        transport_coordinator_->QueueFetchFullMessage(mailbox_id, message_id, &error_message);
    ReloadWorkspace();
    return queued;
}

bool HaikuShellHost::RetryTask(std::string_view task_or_action_id) {
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

LocalDirectoryServiceCatalog& HaikuShellHost::DirectoryServices() {
    return *directory_services_;
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

void HaikuShellHost::ShowMainWindow() {
    if (!main_window_) {
        main_window_ = std::make_unique<HaikuMainWindow>(*this);
    }

    main_window_->Show();
    OpenToolWindow("mailboxes");

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
        const auto messages = message_store_->ListMessages(mailbox.id);
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
                DeliveryStateLabel(message.delivery_state),
                PriorityLabel(message.compose_options.priority),
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
            detail.unread = message.unread;
            detail.download_complete = message.download_complete;
            detail.attachments_omitted = message.attachments_omitted;
            detail.flagged = message.flagged;
            detail.deleted = message.deleted;
            detail.answered = message.answered;
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
            {"drafts", "Drafts", {}, "", MailboxProtocol::kLocal, "", false, true, drafts.size()}, &ignored);
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
        detail.unread = false;
        detail.download_complete = true;
        detail.attachments_omitted = false;
        for (const auto& attachment : draft.attachments) {
            detail.attachments.push_back(BuildAttachmentSummary(attachment));
        }
        workspace_->AddMessageDetail(detail);
    }

    if (main_window_) {
        main_window_->RefreshWorkspace();
    }
}

void HaikuShellHost::EnsureWorkspaceDirectories() {
    std::error_code ignored;
    std::filesystem::create_directories(DataRoot(), ignored);
    std::filesystem::create_directories(DataRoot() / "drafts", ignored);
    std::filesystem::create_directories(DataRoot() / "Stationery", ignored);
    std::filesystem::create_directories(DataRoot() / "Signatures", ignored);

    std::string error_message;
    mailbox_store_->EnsureMailbox({"inbox", "Inbox", {}, "", MailboxProtocol::kLocal, "", false, true, 0},
                                  &error_message);
    mailbox_store_->EnsureMailbox({"out", "Out", {}, "", MailboxProtocol::kLocal, "", false, true, 0},
                                  &error_message);
    mailbox_store_->EnsureMailbox({"drafts", "Drafts", {}, "", MailboxProtocol::kLocal, "", false, true, 0},
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
        return std::filesystem::path(home) / "config" / "settings" / "HermesHemera";
    }
    return std::filesystem::current_path() / "var" / "haiku-shell";
}

std::filesystem::path HaikuShellHost::SettingsPath() const {
    return DataRoot() / "EUDORA.ini";
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

bool HaikuShellHost::OpenToolWindow(std::string_view tool_id) {
    const std::string requested(tool_id);
    for (const auto& window : tool_windows_) {
        if (window && window->ToolId() == requested) {
            window->Refresh();
            if (window->IsHidden()) {
                window->Show();
            } else {
                window->Activate();
            }
            return true;
        }
    }

    std::string title = requested;
    if (requested == "mailboxes") {
        title = "Mailboxes";
    } else if (requested == "task-status") {
        title = "Task Status";
    } else if (requested == "task-errors") {
        title = "Task Errors";
    } else if (requested == "signatures") {
        title = "Signatures";
    } else if (requested == "stationery") {
        title = "Stationery";
    } else if (requested == "nicknames") {
        title = "Nicknames";
    } else if (requested == "personalities") {
        title = "Personalities";
    } else if (requested == "filters") {
        title = "Filters";
    } else if (requested == "filter-report") {
        title = "Filter Report";
    } else if (requested == "directory-services") {
        title = "Directory Services";
    } else if (requested == "file-browser") {
        title = "File Browser";
    } else if (requested == "link-history") {
        title = "Link History";
    }

    auto window = std::make_unique<HaikuToolWindow>(*this, requested, title);
    window->Show();
    tool_windows_.push_back(std::move(window));
    return true;
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
            }

            std::string destination_mailbox = mailbox.id;
            if (result.mark_as_junk) {
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

}  // namespace hermes::haiku_port
