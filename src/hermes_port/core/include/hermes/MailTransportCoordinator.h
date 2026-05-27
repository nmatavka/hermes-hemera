#pragma once

#include <string>
#include <vector>

#include "hermes/AccountService.h"
#include "hermes/CredentialStore.h"
#include "hermes/MailTaskModel.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"
#include "hermes/SyncStateStore.h"
#include "hermes/TlsProvider.h"
#include "hermes/TransportService.h"

namespace hermes {

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
                             MailTaskModel& task_model);

    MailTransportSummary SendQueued();
    MailTransportSummary CheckMail();
    MailTransportSummary SendAndReceive();

private:
    AccountService& account_service_;
    CredentialStore& credential_store_;
    SyncStateStore& sync_state_store_;
    MailboxStore& mailbox_store_;
    MessageStore& message_store_;
    TransportService& transport_service_;
    TlsProvider& tls_provider_;
    MailTaskModel& task_model_;
};

}  // namespace hermes
