#include "TestRegistry.h"

#include "hermes/InMemoryWorkspaceModel.h"

HERMES_TEST(InMemoryWorkspaceModelReturnsMessageDetailsWithAttachments) {
    hermes::InMemoryWorkspaceModel workspace;

    workspace.AddMailbox({"imap:INBOX", "INBOX", 2});
    workspace.AddMessage({"msg-1", "imap:INBOX", "Subject", "sender@example.com", "Preview text", true, 1});
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

    const auto detail = workspace.GetMessageDetail("msg-1");
    HERMES_CHECK(static_cast<bool>(detail));
    HERMES_CHECK_EQ(detail->plain_text_body, std::string("Plain body"));
    HERMES_CHECK(detail->attachments_omitted);
    HERMES_CHECK_EQ(detail->attachments.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(detail->attachments.front().name, std::string("report.pdf"));
    HERMES_CHECK_EQ(detail->attachments.front().fetch_error, std::string("Pending fetch"));
}
