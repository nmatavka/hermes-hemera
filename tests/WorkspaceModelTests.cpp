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
                          hermes::LegacyMessageStatus::kUnread,
                          3,
                          42,
                          true,
                          hermes::PopServerStatus::kFetch,
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
        "<p>Plain body</p>",
        "{\\rtf1\\ansi Plain body}",
        "native",
        hermes::StyledDocumentSource::kHtml,
        hermes::StyledDocumentFidelity::kLossless,
        true,
        false,
        true,
        true,
        false,
        false,
        hermes::LegacyMessageStatus::kUnread,
        3,
        42,
        true,
        hermes::PopServerStatus::kFetch,
        "Fetch pending",
        {{"report.pdf", "application/pdf", 128, true, false, "Pending fetch", "cid-report", "inline"}},
    });

    const auto summary = workspace.GetMessage("msg-1");
    HERMES_CHECK(static_cast<bool>(summary));
    HERMES_CHECK_EQ(summary->attachment_count, static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(summary->status, std::string("received"));
    HERMES_CHECK_EQ(summary->priority, std::string("high"));
    HERMES_CHECK_EQ(summary->legacy_status, hermes::LegacyMessageStatus::kUnread);
    HERMES_CHECK_EQ(summary->label_index, 3);
    HERMES_CHECK_EQ(summary->junk_score, 42);
    HERMES_CHECK(summary->manually_junked);
    HERMES_CHECK_EQ(summary->pop_server_status, hermes::PopServerStatus::kFetch);
    HERMES_CHECK(summary->attachments_omitted);
    HERMES_CHECK(!summary->download_complete);
    HERMES_CHECK_EQ(summary->size, static_cast<std::size_t>(128));
    HERMES_CHECK_EQ(summary->timestamp, static_cast<std::int64_t>(1716500000));

    const auto detail = workspace.GetMessageDetail("msg-1");
    HERMES_CHECK(static_cast<bool>(detail));
    HERMES_CHECK_EQ(detail->plain_text_body, std::string("Plain body"));
    HERMES_CHECK_EQ(detail->html_body, std::string("<p>Plain body</p>"));
    HERMES_CHECK_EQ(detail->rtf_body, std::string("{\\rtf1\\ansi Plain body}"));
    HERMES_CHECK_EQ(detail->paige_native_body, std::string("native"));
    HERMES_CHECK_EQ(detail->styled_source, hermes::StyledDocumentSource::kHtml);
    HERMES_CHECK_EQ(detail->legacy_status, hermes::LegacyMessageStatus::kUnread);
    HERMES_CHECK_EQ(detail->label_index, 3);
    HERMES_CHECK_EQ(detail->junk_score, 42);
    HERMES_CHECK(detail->manually_junked);
    HERMES_CHECK_EQ(detail->pop_server_status, hermes::PopServerStatus::kFetch);
    HERMES_CHECK(detail->attachments_omitted);
    HERMES_CHECK_EQ(detail->attachments.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(detail->attachments.front().name, std::string("report.pdf"));
    HERMES_CHECK_EQ(detail->attachments.front().fetch_error, std::string("Pending fetch"));
    HERMES_CHECK_EQ(detail->attachments.front().content_id, std::string("cid-report"));
    HERMES_CHECK_EQ(detail->attachments.front().disposition, std::string("inline"));

    const auto mailboxes = workspace.Mailboxes();
    HERMES_CHECK_EQ(mailboxes.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(mailboxes.front().parent_id, std::string("account:imap"));
    HERMES_CHECK_EQ(mailboxes.front().account_id, std::string("imap"));
    HERMES_CHECK_EQ(mailboxes.front().protocol, std::string("imap"));
    HERMES_CHECK(mailboxes.front().system_mailbox);
    HERMES_CHECK(mailboxes.front().is_remote);
}
