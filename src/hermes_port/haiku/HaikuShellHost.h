#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hermes/AccountService.h"
#include "hermes/CredentialStore.h"
#include "hermes/DraftStore.h"
#include "hermes/DirectoryServiceCatalog.h"
#include "hermes/ImapActionStore.h"
#include "hermes/IniSettingsStore.h"
#include "hermes/FilterReportStore.h"
#include "hermes/FilterStore.h"
#include "hermes/FilesystemPluginHost.h"
#include "hermes/HelpCatalog.h"
#include "hermes/ImportService.h"
#include "hermes/LinkHistoryStore.h"
#include "hermes/MailTransferSettings.h"
#include "hermes/MailboxUiSettings.h"
#include "hermes/MailboxWorkflow.h"
#include "hermes/MailTaskModel.h"
#include "hermes/MailTransportCoordinator.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"
#include "hermes/AddressBookService.h"
#include "hermes/NicknameStore.h"
#include "hermes/OAuthSupport.h"
#include "hermes/OpenSslTlsProvider.h"
#include "hermes/PaigeRuntime.h"
#include "hermes/PopServerStatus.h"
#include "hermes/SyncStateStore.h"
#include "hermes/TransportService.h"
#include "hermes/InMemoryWorkspaceModel.h"
#include "hermes/SignatureStore.h"
#include "hermes/ShellHost.h"
#include "hermes/ShellBehaviorSettings.h"
#include "hermes/SearchService.h"
#include "hermes/StationeryStore.h"
#include "HaikuHermesImports.h"

namespace hemera::haiku {

class HaikuMainWindow;
class HaikuComposeWindow;
class HaikuMessageWindow;
class HaikuWazooWindow;

class HaikuShellHost final : public ShellHost {
public:
    enum class WindowArrangeMode {
        kCascade,
        kTileHorizontally,
        kTileVertically,
        kArrange,
    };

    enum class MessageResponseKind {
        kReply,
        kReplyAll,
        kForward,
        kRedirect,
        kSendAgain,
    };

    struct SearchRequest {
        enum class Scope {
            kAllMailboxes,
            kCurrentMailbox,
            kCurrentFolder,
        };

        std::string term;
        Scope scope = Scope::kAllMailboxes;
        std::string anchor_mailbox_id;
    };

    HaikuShellHost();
    ~HaikuShellHost() override;

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
    bool PurgeMailbox(std::string_view mailbox_id) override;
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
    FilesystemOAuthTokenStore& OAuthTokens();
    FilesystemSyncStateStore& SyncState();
    FilesystemImapActionStore& ImapActions();
    InMemoryMailTaskModel& Tasks();
    FilesystemDraftStore& Drafts();
    FilesystemMailboxStore& Mailboxes();
    FilesystemMessageStore& Messages();
    FlatFileNicknameStore& Nicknames();
    FilesystemStationeryStore& Stationery();
    FilesystemSignatureStore& Signatures();
    MemoryAddressBookService& AddressBook();
    FilesystemFilterStore& Filters();
    FilesystemFilterReportStore& FilterReport();
    FilesystemLinkHistoryStore& LinkHistory();
    FilesystemPluginHost& Plugins();
    LocalDirectoryServiceCatalog& DirectoryServices();
    LegacyHelpCatalog& Help();
    LegacyImportService& Importer();
    SimpleSearchService& Search();
    OAuthDeviceFlowService& OAuthService();
    PaigeRuntime& Runtime();
    MailTransportCoordinator& TransportCoordinator();
    const std::optional<ComposeMessage>& PendingComposerMessage() const;
    std::optional<MessageDetail> WorkspaceMessageDetail(std::string_view message_id) const;
    std::vector<ImapActionRecord> QueuedImapActions() const;
    std::filesystem::path DataRootPath() const;
    std::filesystem::path SettingsFilePath() const;
    std::string ActiveMailboxId() const;
    std::string ActiveMessageId() const;
    void UpdateWazooWindowState(std::string_view group_id, const hermes::WazooWindowState& state);
    void SetWazooWindowVisible(std::string_view group_id, bool visible);

    void ShowMainWindow();
    void ReloadWorkspace();
    bool PersistSettings(std::string* error_message = nullptr);
    void RecordAttachmentLaunch(std::string_view title,
                                const std::filesystem::path& path,
                                std::string_view source_context);
    bool ShowMessage(std::string_view mailbox_id, std::string_view message_id);
    bool OpenMessageWindow(std::string_view mailbox_id, std::string_view message_id);
    bool OpenToolWindow(std::string_view tool_id);
    bool OpenHelpWindow();
    bool OpenImportWindow();
    bool RevealHelpFiles();
    bool RescanPlugins(std::string* error_message = nullptr);
    bool ReloadImportedShellState(std::string_view settings_snapshot_name,
                                  std::string* error_message = nullptr);
    std::filesystem::path UserPluginRootPath() const;
    std::filesystem::path AppPluginRootPath() const;
    std::vector<std::filesystem::path> PluginDiscoveryRoots() const;
    std::vector<std::string> PluginScanErrors() const;
    bool MailboxAutoSyncEnabled(std::string_view mailbox_id) const;
    bool SetMailboxAutoSyncEnabled(std::string_view mailbox_id, bool enabled);
    bool MailboxShowsDeleted(std::string_view mailbox_id) const;
    bool SetMailboxShowsDeleted(std::string_view mailbox_id, bool show_deleted);
    bool ResyncMailboxTree(std::string_view mailbox_id);
    MailboxUiSettings MailboxUi() const;
    ShellBehaviorSettings ShellBehavior() const;
    MailTransferSettings MailTransfer() const;
    MailTransportSummary ExecuteMailTransfer(const MailTransferRequest& request);
    bool SetOfflineMode(bool offline);
    bool ClearMailboxContents(std::string_view mailbox_id, std::string* error_message = nullptr);
    bool CompactMailboxes(std::string* error_message = nullptr);
    bool ForgetAllCredentialsAndTokens(std::string* error_message = nullptr);
    bool UpdateAccountPasswords(std::string_view account_id,
                                std::string_view incoming_password,
                                std::string_view outgoing_password,
                                std::string* error_message = nullptr);
    bool SendActiveWindowToBack();
    bool ArrangeManagedWindows(WindowArrangeMode mode);
    bool CloseAllManagedWindows();
    bool SaveOpenWindowLayout(std::string* error_message = nullptr);
    bool SetLegacyStatusForMessages(std::string_view mailbox_id,
                                    const std::vector<std::string>& message_ids,
                                    LegacyMessageStatus status);
    bool SetLabelForMessages(std::string_view mailbox_id,
                             const std::vector<std::string>& message_ids,
                             int label_index);
    bool SetPopServerStatusForMessages(std::string_view mailbox_id,
                                       const std::vector<std::string>& message_ids,
                                       PopServerStatus status);
    bool ChangeQueueingForMessages(std::string_view mailbox_id,
                                   const std::vector<std::string>& message_ids,
                                   int delay_seconds);
    bool ApplyJunkActionToMessages(std::string_view mailbox_id,
                                   const std::vector<std::string>& message_ids,
                                   MailboxJunkAction action);
    bool ApplyFiltersToMessages(std::string_view mailbox_id, const std::vector<std::string>& message_ids);
    std::optional<FilterRule> CreateManualFilterFromMessages(std::string_view mailbox_id,
                                                             const std::vector<std::string>& message_ids,
                                                             std::string* error_message = nullptr);
    bool ClearCachedImapMessages(std::string_view mailbox_id, const std::vector<std::string>& message_ids);
    bool FetchDefaultImapMessages(std::string_view mailbox_id, const std::vector<std::string>& message_ids);
    bool RedownloadImapMessages(std::string_view mailbox_id,
                                const std::vector<std::string>& message_ids,
                                bool full);
    void SetActiveMessageContext(std::string mailbox_id, std::string message_id);
    std::optional<ComposeMessage> BuildResponseMessage(MessageResponseKind kind,
                                                      std::string_view mailbox_id,
                                                      std::string_view message_id,
                                                      std::string_view stationery_name = {}) const;
    void QueuePendingSearch(SearchRequest request);
    std::optional<SearchRequest> TakePendingSearch();
    void QueuePendingDirectoryQuery(std::string query);
    std::optional<std::string> TakePendingDirectoryQuery();

private:
    void LoadBootstrapAccounts();
    void LoadToolData();
    void ShowComposeWindow(const ComposeMessage& message);
    void EnsureWorkspaceDirectories();
    void BootstrapTemplatesIfNeeded();
    void ApplyPendingFilters();
    std::optional<MailboxRecord> FindMailboxRole(std::string_view account_id, std::string_view role_name) const;
    bool ReloadHelpCatalog(std::string* error_message = nullptr);
    void RestoreWazooWindows();
    std::filesystem::path DataRoot() const;
    std::filesystem::path SettingsPath() const;
    void RememberRecentMailbox(std::string_view mailbox_id);

    std::unique_ptr<IniSettingsStore> settings_;
    std::unique_ptr<InMemoryWorkspaceModel> workspace_;
    std::unique_ptr<LegacyAccountService> account_service_;
    std::unique_ptr<FilesystemCredentialStore> credential_store_;
    std::unique_ptr<FilesystemOAuthTokenStore> oauth_token_store_;
    std::unique_ptr<FilesystemSyncStateStore> sync_state_store_;
    std::unique_ptr<FilesystemImapActionStore> imap_action_store_;
    std::unique_ptr<InMemoryMailTaskModel> task_model_;
    std::unique_ptr<FilesystemDraftStore> draft_store_;
    std::unique_ptr<FilesystemMailboxStore> mailbox_store_;
    std::unique_ptr<FilesystemMessageStore> message_store_;
    std::unique_ptr<FlatFileNicknameStore> nickname_store_;
    std::unique_ptr<FilesystemStationeryStore> stationery_store_;
    std::unique_ptr<FilesystemSignatureStore> signature_store_;
    std::unique_ptr<MemoryAddressBookService> address_book_service_;
    std::unique_ptr<FilesystemFilterStore> filter_store_;
    std::unique_ptr<FilesystemFilterReportStore> filter_report_store_;
    std::unique_ptr<FilesystemLinkHistoryStore> link_history_store_;
    std::unique_ptr<FilesystemPluginHost> plugin_host_;
    std::unique_ptr<LocalDirectoryServiceCatalog> directory_services_;
    std::unique_ptr<LegacyHelpCatalog> help_catalog_;
    std::unique_ptr<LegacyImportService> import_service_;
    std::unique_ptr<SimpleSearchService> search_service_;
    std::unique_ptr<OpenSslTlsProvider> tls_provider_;
    std::unique_ptr<SocketTransportService> transport_service_;
    std::unique_ptr<TransportOAuthHttpClient> oauth_http_client_;
    std::unique_ptr<OAuthDeviceFlowService> oauth_device_flow_service_;
    std::unique_ptr<MailTransportCoordinator> transport_coordinator_;
    std::unique_ptr<PaigeRuntime> paige_runtime_;
    std::unique_ptr<HaikuMainWindow> main_window_;
    std::vector<std::unique_ptr<HaikuWazooWindow>> wazoo_windows_;
    std::vector<std::unique_ptr<HaikuComposeWindow>> compose_windows_;
    std::vector<std::unique_ptr<HaikuMessageWindow>> message_windows_;
    std::unique_ptr<class HaikuHelpWindow> help_window_;
    std::unique_ptr<class HaikuImportWindow> import_window_;
    std::optional<ComposeMessage> pending_composer_message_;
    std::string active_mailbox_id_;
    std::string active_message_id_;
    std::optional<SearchRequest> pending_search_request_;
    std::optional<std::string> pending_directory_query_;
    std::vector<std::string> plugin_scan_errors_;
};

}  // namespace hemera::haiku
