#include "TestRegistry.h"

#include "hermes/InMemoryWorkspaceModel.h"

HERMES_TEST(InMemoryWorkspaceModelReturnsMessageDetailsWithAttachments) {
    hermes::InMemoryWorkspaceModel workspace;

    workspace.AddMailbox(
        {"imap:INBOX", "INBOX", 2, "account:imap", "imap", "imap", true, true});
    workspace.AddMessage({"msg-1",
                          "imap:INBOX",
                          "Subject",
                          "sender@example.com",
                          "Preview text",
                          true,
                          1,
                          "received",
                          "high",
                          true,
                          false,
                          128,
                          1716500000});
    workspace.AddMessageDetail({
        "msg-1",
        "imap:INBOX",
        "Subject",
        "sender@example.com",
        "recipient@example.com",
        "Preview text",
        "Plain body",
        true,
        false,
        true,
        true,
        false,
        false,
        "Fetch pending",
        {{"report.pdf", "application/pdf", 128, true, false, "Pending fetch"}},
    });

    const auto summary = workspace.GetMessage("msg-1");
    HERMES_CHECK(static_cast<bool>(summary));
    HERMES_CHECK_EQ(summary->attachment_count, static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(summary->status, std::string("received"));
    HERMES_CHECK_EQ(summary->priority, std::string("high"));
    HERMES_CHECK(summary->attachments_omitted);
    HERMES_CHECK(!summary->download_complete);
    HERMES_CHECK_EQ(summary->size, static_cast<std::size_t>(128));
    HERMES_CHECK_EQ(summary->timestamp, static_cast<std::int64_t>(1716500000));

    const auto detail = workspace.GetMessageDetail("msg-1");
    HERMES_CHECK(static_cast<bool>(detail));
    HERMES_CHECK_EQ(detail->plain_text_body, std::string("Plain body"));
    HERMES_CHECK(detail->attachments_omitted);
    HERMES_CHECK_EQ(detail->attachments.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(detail->attachments.front().name, std::string("report.pdf"));
    HERMES_CHECK_EQ(detail->attachments.front().fetch_error, std::string("Pending fetch"));

    const auto mailboxes = workspace.Mailboxes();
    HERMES_CHECK_EQ(mailboxes.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(mailboxes.front().parent_id, std::string("account:imap"));
    HERMES_CHECK_EQ(mailboxes.front().account_id, std::string("imap"));
    HERMES_CHECK_EQ(mailboxes.front().protocol, std::string("imap"));
    HERMES_CHECK(mailboxes.front().system_mailbox);
    HERMES_CHECK(mailboxes.front().is_remote);
}
