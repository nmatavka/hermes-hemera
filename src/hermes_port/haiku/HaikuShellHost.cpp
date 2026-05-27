#include "HaikuShellHost.h"

#include <Application.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <system_error>

#include "HaikuComposeWindow.h"
#include "HaikuMainWindow.h"

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
      stationery_store_(std::make_unique<FilesystemStationeryStore>()),
      signature_store_(std::make_unique<FilesystemSignatureStore>()),
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
      paige_runtime_(std::make_unique<PaigeRuntime>()) {
    std::string ignored;
    (void)paige_runtime_->Initialize(&ignored);
    EnsureWorkspaceDirectories();
    LoadBootstrapAccounts();
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
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::SendAndReceive() {
    const auto summary = transport_coordinator_->SendAndReceive();
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::RefreshMailbox(std::string_view mailbox_id) {
    const auto summary = transport_coordinator_->RefreshMailbox(mailbox_id, false);
    ReloadWorkspace();
    return summary.success;
}

bool HaikuShellHost::ResyncMailbox(std::string_view mailbox_id) {
    const auto summary = transport_coordinator_->RefreshMailbox(mailbox_id, true);
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

FilesystemStationeryStore& HaikuShellHost::Stationery() {
    return *stationery_store_;
}

FilesystemSignatureStore& HaikuShellHost::Signatures() {
    return *signature_store_;
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

void HaikuShellHost::ShowMainWindow() {
    if (!main_window_) {
        main_window_ = std::make_unique<HaikuMainWindow>(*this);
    }

    main_window_->Show();

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
        workspace_->AddMailbox({mailbox.id, mailbox.display_name, mailbox.message_count});
        for (const auto& message : message_store_->ListMessages(mailbox.id)) {
            const std::string preview = PreviewText(message.plain_text_body);
            workspace_->AddMessage({
                message.id,
                mailbox.id,
                message.subject,
                message.sender,
                preview,
                message.unread,
                message.attachments.size(),
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

    std::string error_message;
    mailbox_store_->EnsureMailbox({"inbox", "Inbox", {}, "", MailboxProtocol::kLocal, "", false, true, 0},
                                  &error_message);
    mailbox_store_->EnsureMailbox({"out", "Out", {}, "", MailboxProtocol::kLocal, "", false, true, 0},
                                  &error_message);
    mailbox_store_->EnsureMailbox({"drafts", "Drafts", {}, "", MailboxProtocol::kLocal, "", false, true, 0},
                                  &error_message);

    const auto stationery_root = SourceRoot() / "tests" / "fixtures" / "legacy" / "compose" / "stationery";
    const auto signature_root = SourceRoot() / "tests" / "fixtures" / "legacy" / "compose" / "signatures";
    stationery_store_->Discover(stationery_root, nullptr);
    signature_store_->Discover(signature_root, nullptr);
}

void HaikuShellHost::LoadBootstrapAccounts() {
    const auto profile = SourceRoot() / "tests" / "fixtures" / "legacy" / "profile_snapshots" / "Eudora.box";
    std::string ignored;
    account_service_->LoadFromIniFile(profile, &ignored);
}

std::filesystem::path HaikuShellHost::DataRoot() const {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "config" / "settings" / "HermesHemera";
    }
    return std::filesystem::current_path() / "var" / "haiku-shell";
}

}  // namespace hermes::haiku_port
