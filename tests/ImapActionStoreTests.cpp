#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/ImapActionStore.h"

HERMES_TEST(FilesystemImapActionStoreRoundTripsPendingAction) {
    hermes::tests::ScopedTempDirectory temp("hemera-imap-actions");
    hermes::FilesystemImapActionStore store(temp.Path());

    hermes::ImapActionRecord action;
    action.id = "imap-action-1";
    action.kind = hermes::ImapActionKind::kMove;
    action.state = hermes::ImapActionState::kPending;
    action.account_id = "primary";
    action.mailbox_id = "primary:INBOX";
    action.remote_mailbox = "INBOX";
    action.message_id = "msg-1";
    action.remote_message_id = "101";
    action.destination_mailbox_id = "primary:Archive";
    action.destination_remote_mailbox = "Archive";
    action.created_at = 100;
    action.updated_at = 100;
    action.attempts = 1;

    std::string error_message;
    HERMES_CHECK(store.SaveAction(action, &error_message));

    const auto loaded = store.GetAction("imap-action-1");
    HERMES_CHECK(static_cast<bool>(loaded));
    HERMES_CHECK_EQ(loaded->kind, hermes::ImapActionKind::kMove);
    HERMES_CHECK_EQ(loaded->destination_remote_mailbox, std::string("Archive"));

    const auto listed = store.ListActions();
    HERMES_CHECK_EQ(listed.size(), static_cast<std::size_t>(1));
}

HERMES_TEST(FilesystemImapActionStoreRoundTripsMailboxExpungeAction) {
    hermes::tests::ScopedTempDirectory temp("hemera-imap-expunge-actions");
    hermes::FilesystemImapActionStore store(temp.Path());

    hermes::ImapActionRecord action;
    action.id = "imap-expunge-1";
    action.kind = hermes::ImapActionKind::kExpungeMailbox;
    action.state = hermes::ImapActionState::kPending;
    action.account_id = "primary";
    action.mailbox_id = "primary:INBOX";
    action.remote_mailbox = "INBOX";
    action.created_at = 250;
    action.updated_at = 250;

    std::string error_message;
    HERMES_CHECK(store.SaveAction(action, &error_message));

    const auto loaded = store.GetAction("imap-expunge-1");
    HERMES_CHECK(static_cast<bool>(loaded));
    HERMES_CHECK_EQ(loaded->kind, hermes::ImapActionKind::kExpungeMailbox);
    HERMES_CHECK_EQ(loaded->remote_mailbox, std::string("INBOX"));
}
