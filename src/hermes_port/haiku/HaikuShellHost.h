#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hermes/AccountService.h"
#include "hermes/CredentialStore.h"
#include "hermes/DraftStore.h"
#include "hermes/ImapActionStore.h"
#include "hermes/IniSettingsStore.h"
#include "hermes/MailTaskModel.h"
#include "hermes/MailTransportCoordinator.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"
#include "hermes/OpenSslTlsProvider.h"
#include "hermes/PaigeRuntime.h"
#include "hermes/SyncStateStore.h"
#include "hermes/TransportService.h"
#include "hermes/InMemoryWorkspaceModel.h"
#include "hermes/SignatureStore.h"
#include "hermes/ShellHost.h"
#include "hermes/StationeryStore.h"

namespace hermes::haiku_port {

class HaikuMainWindow;
class HaikuComposeWindow;

class HaikuShellHost final : public ShellHost {
public:
    HaikuShellHost();

    int Run() override;
    bool OpenMailbox(std::string_view mailbox_id) override;
    bool OpenComposer(const ComposeMessage& message) override;
    bool SendQueued() override;
    bool CheckMail() override;
    bool SendAndReceive() override;
    bool RefreshMailbox(std::string_view mailbox_id) override;
    bool ResyncMailbox(std::string_view mailbox_id) override;
    bool DeleteMessage(std::string_view mailbox_id, std::string_view message_id) override;
    bool UndeleteMessage(std::string_view mailbox_id, std::string_view message_id) override;
    bool MoveMessage(std::string_view mailbox_id,
                     std::string_view message_id,
                     std::string_view destination_mailbox_id) override;
    bool CopyMessage(std::string_view mailbox_id,
                     std::string_view message_id,
                     std::string_view destination_mailbox_id) override;
    bool CreateRemoteMailbox(std::string_view account_id, std::string_view remote_name) override;
    bool RenameRemoteMailbox(std::string_view mailbox_id, std::string_view new_remote_name) override;
    bool DeleteRemoteMailbox(std::string_view mailbox_id) override;
    std::optional<std::filesystem::path> AttachmentPath(std::string_view mailbox_id,
                                                        std::string_view message_id,
                                                        std::size_t attachment_index) const override;
    bool SaveAttachment(std::string_view mailbox_id,
                        std::string_view message_id,
                        std::size_t attachment_index,
                        const std::filesystem::path& destination_path) override;
    bool SaveAllAttachments(std::string_view mailbox_id,
                            std::string_view message_id,
                            const std::filesystem::path& destination_directory) override;
    bool FetchAttachment(std::string_view mailbox_id,
                         std::string_view message_id,
                         std::size_t attachment_index) override;
    bool FetchFullMessage(std::string_view mailbox_id, std::string_view message_id) override;
    bool RetryTask(std::string_view task_or_action_id) override;
    bool CancelTask(std::string_view task_or_action_id) override;
    bool StopActiveTasks() override;

    InMemoryWorkspaceModel& Workspace();
    LegacyAccountService& Accounts();
    IniSettingsStore& Settings();
    FilesystemCredentialStore& Credentials();
    FilesystemSyncStateStore& SyncState();
    FilesystemImapActionStore& ImapActions();
    InMemoryMailTaskModel& Tasks();
    FilesystemDraftStore& Drafts();
    FilesystemMailboxStore& Mailboxes();
    FilesystemMessageStore& Messages();
    FilesystemStationeryStore& Stationery();
    FilesystemSignatureStore& Signatures();
    PaigeRuntime& Runtime();
    MailTransportCoordinator& TransportCoordinator();
    const std::optional<ComposeMessage>& PendingComposerMessage() const;

    void ShowMainWindow();
    void ReloadWorkspace();

private:
    void LoadBootstrapAccounts();
    void ShowComposeWindow(const ComposeMessage& message);
    void EnsureWorkspaceDirectories();
    std::filesystem::path DataRoot() const;

    std::unique_ptr<IniSettingsStore> settings_;
    std::unique_ptr<InMemoryWorkspaceModel> workspace_;
    std::unique_ptr<LegacyAccountService> account_service_;
    std::unique_ptr<FilesystemCredentialStore> credential_store_;
    std::unique_ptr<FilesystemSyncStateStore> sync_state_store_;
    std::unique_ptr<FilesystemImapActionStore> imap_action_store_;
    std::unique_ptr<InMemoryMailTaskModel> task_model_;
    std::unique_ptr<FilesystemDraftStore> draft_store_;
    std::unique_ptr<FilesystemMailboxStore> mailbox_store_;
    std::unique_ptr<FilesystemMessageStore> message_store_;
    std::unique_ptr<FilesystemStationeryStore> stationery_store_;
    std::unique_ptr<FilesystemSignatureStore> signature_store_;
    std::unique_ptr<OpenSslTlsProvider> tls_provider_;
    std::unique_ptr<SocketTransportService> transport_service_;
    std::unique_ptr<MailTransportCoordinator> transport_coordinator_;
    std::unique_ptr<PaigeRuntime> paige_runtime_;
    std::unique_ptr<HaikuMainWindow> main_window_;
    std::vector<std::unique_ptr<HaikuComposeWindow>> compose_windows_;
    std::optional<ComposeMessage> pending_composer_message_;
    std::string active_mailbox_id_;
};

}  // namespace hermes::haiku_port
