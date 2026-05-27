#include "HaikuShellHost.h"

#include <Application.h>
#include <algorithm>
#include <cstdlib>
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
                                                                        *task_model_)),
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

bool HaikuShellHost::StopActiveTasks() {
    return false;
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
            workspace_->AddMessage({
                message.id,
                mailbox.id,
                message.subject,
                message.sender,
                message.plain_text_body.substr(0, std::min<std::size_t>(message.plain_text_body.size(), 80)),
                message.unread,
                message.attachments.size(),
            });
        }
    }

    const auto drafts = draft_store_->ListDrafts();
    if (!drafts.empty() && !mailbox_store_->GetMailbox("drafts")) {
        std::string ignored;
        mailbox_store_->EnsureMailbox(
            {"drafts", "Drafts", {}, "", MailboxProtocol::kLocal, "", false, true, drafts.size()}, &ignored);
    }
    for (const auto& draft : drafts) {
        workspace_->AddMessage({
            draft.id,
            "drafts",
            draft.headers.subject.empty() ? "(No subject)" : draft.headers.subject,
            draft.headers.from_persona,
            draft.body.plain_text.substr(0, std::min<std::size_t>(draft.body.plain_text.size(), 80)),
            false,
            0,
        });
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
