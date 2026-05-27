#pragma once

#include <cstddef>
#include <atomic>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/AccountService.h"
#include "hermes/CredentialStore.h"
#include "hermes/ImapActionStore.h"
#include "hermes/MailTaskModel.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"
#include "hermes/SyncStateStore.h"
#include "hermes/TlsProvider.h"
#include "hermes/TransportService.h"

namespace hermes {

class GssapiEngine;

struct MailTransportSummary {
    bool success = false;
    std::size_t messages_sent = 0;
    std::size_t messages_received = 0;
    std::size_t mailboxes_discovered = 0;
    std::vector<std::string> warnings;
    std::string error_message;
};

class MailTransportCoordinator {
public:
    MailTransportCoordinator(AccountService& account_service,
                             CredentialStore& credential_store,
                             SyncStateStore& sync_state_store,
                             MailboxStore& mailbox_store,
                             MessageStore& message_store,
                             TransportService& transport_service,
                             TlsProvider& tls_provider,
                             MailTaskModel& task_model,
                             ImapActionStore* imap_action_store = nullptr,
                             const GssapiEngine* gssapi_engine = nullptr);

    MailTransportSummary SendQueued();
    MailTransportSummary CheckMail();
    MailTransportSummary SendAndReceive();
    MailTransportSummary RefreshMailbox(std::string_view mailbox_id, bool full_resync);
    bool QueueDeleteMessage(std::string_view mailbox_id,
                            std::string_view message_id,
                            std::string* error_message = nullptr);
    bool QueueUndeleteMessage(std::string_view mailbox_id,
                              std::string_view message_id,
                              std::string* error_message = nullptr);
    bool QueueMoveMessage(std::string_view mailbox_id,
                          std::string_view message_id,
                          std::string_view destination_mailbox_id,
                          std::string* error_message = nullptr);
    bool QueueCopyMessage(std::string_view mailbox_id,
                          std::string_view message_id,
                          std::string_view destination_mailbox_id,
                          std::string* error_message = nullptr);
    bool QueueCreateMailbox(std::string_view account_id,
                            std::string_view remote_name,
                            std::string* error_message = nullptr);
    bool QueueRenameMailbox(std::string_view mailbox_id,
                            std::string_view new_remote_name,
                            std::string* error_message = nullptr);
    bool QueueDeleteMailbox(std::string_view mailbox_id, std::string* error_message = nullptr);
    bool QueueFetchAttachment(std::string_view mailbox_id,
                              std::string_view message_id,
                              std::size_t attachment_index,
                              std::string* error_message = nullptr);
    bool QueueFetchFullMessage(std::string_view mailbox_id,
                               std::string_view message_id,
                               std::string* error_message = nullptr);
    bool RetryImapAction(std::string_view action_id, std::string* error_message = nullptr);
    bool CancelImapAction(std::string_view action_id, std::string* error_message = nullptr);
    bool StopActiveTasks();

private:
    AccountService& account_service_;
    CredentialStore& credential_store_;
    SyncStateStore& sync_state_store_;
    MailboxStore& mailbox_store_;
    MessageStore& message_store_;
    TransportService& transport_service_;
    TlsProvider& tls_provider_;
    MailTaskModel& task_model_;
    ImapActionStore* imap_action_store_ = nullptr;
    const GssapiEngine* gssapi_engine_ = nullptr;
    std::atomic<bool> stop_requested_{false};
};

}  // namespace hermes
