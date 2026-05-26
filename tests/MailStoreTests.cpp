#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"

HERMES_TEST(FilesystemMailboxStoreCreatesAndListsMailboxes) {
    hermes::tests::ScopedTempDirectory temp("hermes-mailboxes");
    hermes::FilesystemMailboxStore store(temp.Path());

    std::string error_message;
    HERMES_CHECK(store.EnsureMailbox({"inbox", "Inbox", {}, true, 0}, &error_message));
    HERMES_CHECK(store.EnsureMailbox({"archive", "Archive", {}, false, 0}, &error_message));

    const auto mailboxes = store.ListMailboxes();
    HERMES_CHECK_EQ(mailboxes.size(), static_cast<std::size_t>(2));

    const auto inbox = store.GetMailbox("inbox");
    HERMES_CHECK(static_cast<bool>(inbox));
    HERMES_CHECK_EQ(inbox->display_name, std::string("Inbox"));
    HERMES_CHECK(inbox->system_mailbox);
}

HERMES_TEST(FilesystemMessageStoreRoundTripsPlainAndHtmlBodies) {
    hermes::tests::ScopedTempDirectory temp("hermes-messages");
    hermes::FilesystemMessageStore store(temp.Path());

    hermes::MessageRecord message;
    message.id = "42";
    message.mailbox_id = "inbox";
    message.subject = "Hello";
    message.sender = "alice@example.com";
    message.recipients = "bob@example.com";
    message.plain_text_body = "Plain version";
    message.html_body = "<p>Html version</p>";
    message.unread = false;

    std::string error_message;
    HERMES_CHECK(store.SaveMessage(message, &error_message));

    const auto listed = store.ListMessages("inbox");
    HERMES_CHECK_EQ(listed.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(listed.front().subject, std::string("Hello"));

    const auto loaded = store.GetMessage("inbox", "42");
    HERMES_CHECK(static_cast<bool>(loaded));
    HERMES_CHECK_EQ(loaded->sender, std::string("alice@example.com"));
    HERMES_CHECK_EQ(loaded->plain_text_body, std::string("Plain version"));
    HERMES_CHECK_EQ(loaded->html_body, std::string("<p>Html version</p>"));
    HERMES_CHECK(!loaded->unread);
}
