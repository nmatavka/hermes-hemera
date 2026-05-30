#include "TestPaths.h"
#include "TestRegistry.h"

#include <algorithm>
#include <fstream>

#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"

HERMES_TEST(FilesystemMailboxStoreCreatesAndListsMailboxes) {
    hermes::tests::ScopedTempDirectory temp("hemera-mailboxes");
    hermes::FilesystemMailboxStore store(temp.Path());

    std::string error_message;
    HERMES_CHECK(
        store.EnsureMailbox({"inbox", "Inbox", {}, "primary", hermes::MailboxProtocol::kLocal, "", false, true, 0},
                            &error_message));
    HERMES_CHECK(store.EnsureMailbox(
        {"archive", "Archive", {}, "primary", hermes::MailboxProtocol::kImap, "Archive", true, false, 0},
        &error_message));

    const auto mailboxes = store.ListMailboxes();
    HERMES_CHECK(mailboxes.size() >= static_cast<std::size_t>(7));

    const auto inbox = store.GetMailbox("inbox");
    HERMES_CHECK(static_cast<bool>(inbox));
    HERMES_CHECK_EQ(inbox->display_name, std::string("Inbox"));
    HERMES_CHECK(inbox->system_mailbox);
    HERMES_CHECK_EQ(inbox->path.extension(), std::filesystem::path(".mbx"));

    const auto archive_it = std::find_if(mailboxes.begin(), mailboxes.end(), [](const hermes::MailboxRecord& mailbox) {
        return mailbox.protocol == hermes::MailboxProtocol::kImap && mailbox.remote_name == "Archive";
    });
    HERMES_CHECK(archive_it != mailboxes.end());
    const auto archive = store.GetMailbox(archive_it->id);
    HERMES_CHECK(static_cast<bool>(archive));
    HERMES_CHECK_EQ(archive->protocol, hermes::MailboxProtocol::kImap);
    HERMES_CHECK(archive->is_remote);
    HERMES_CHECK_EQ(archive->path.extension(), std::filesystem::path(".mbx"));
}

HERMES_TEST(FilesystemMessageStoreRoundTripsPlainAndHtmlBodies) {
    hermes::tests::ScopedTempDirectory temp("hemera-messages");
    hermes::FilesystemMessageStore store(temp.Path());

    hermes::MessageRecord message;
    message.id = "42";
    message.mailbox_id = "inbox";
    message.subject = "Hello";
    message.sender = "alice@example.com";
    message.recipients = "bob@example.com";
    message.plain_text_body = "Plain version";
    message.html_body = "<p>Html version</p>";
    message.rtf_body = "{\\rtf1\\ansi Html version}";
    message.paige_native_body = "native-body";
    message.styled_source = hermes::StyledDocumentSource::kHtml;
    message.styled_fidelity = hermes::StyledDocumentFidelity::kLossless;
    message.account_id = "primary";
    message.delivery_state = hermes::MessageDeliveryState::kQueued;
    message.legacy_status = hermes::LegacyMessageStatus::kQueued;
    message.remote_id = "101";
    message.remote_mailbox = "INBOX";
    message.attachments_omitted = true;
    message.flagged = true;
    message.filters_applied = true;
    message.label_index = 4;
    message.junk_score = 37;
    message.manually_junked = true;
    message.pop_server_status = hermes::PopServerStatus::kFetchDelete;
    message.attachments.push_back({"report.pdf", "application/pdf", 128, false});
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
    HERMES_CHECK_EQ(loaded->rtf_body, std::string("{\\rtf1\\ansi Html version}"));
    HERMES_CHECK_EQ(loaded->paige_native_body, std::string("native-body"));
    HERMES_CHECK_EQ(loaded->styled_source, hermes::StyledDocumentSource::kHtml);
    HERMES_CHECK_EQ(loaded->styled_fidelity, hermes::StyledDocumentFidelity::kLossless);
    HERMES_CHECK_EQ(loaded->account_id, std::string("primary"));
    HERMES_CHECK_EQ(loaded->delivery_state, hermes::MessageDeliveryState::kQueued);
    HERMES_CHECK_EQ(loaded->legacy_status, hermes::LegacyMessageStatus::kQueued);
    HERMES_CHECK(loaded->attachments_omitted);
    HERMES_CHECK(loaded->flagged);
    HERMES_CHECK(loaded->filters_applied);
    HERMES_CHECK_EQ(loaded->label_index, 4);
    HERMES_CHECK_EQ(loaded->junk_score, 37);
    HERMES_CHECK(loaded->manually_junked);
    HERMES_CHECK_EQ(loaded->pop_server_status, hermes::PopServerStatus::kFetchDelete);
    HERMES_CHECK_EQ(loaded->attachments.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(loaded->attachments.front().name, std::string("report.pdf"));
    HERMES_CHECK(!loaded->unread);
}

HERMES_TEST(FilesystemMessageStoreRoundTripsRtfOnlyBodiesWithoutPromotingToHtml) {
    hermes::tests::ScopedTempDirectory temp("hemera-messages-rtf");
    hermes::FilesystemMessageStore store(temp.Path());

    hermes::MessageRecord message;
    message.id = "88";
    message.mailbox_id = "inbox";
    message.subject = "RTF only";
    message.sender = "alice@example.com";
    message.recipients = "bob@example.com";
    message.plain_text_body = "Rich plain";
    message.rtf_body = "{\\rtf1\\ansi Rich plain}";
    message.styled_source = hermes::StyledDocumentSource::kRtf;

    std::string error_message;
    HERMES_CHECK(store.SaveMessage(message, &error_message));

    const auto loaded = store.GetMessage("inbox", "88");
    HERMES_CHECK(static_cast<bool>(loaded));
    HERMES_CHECK_EQ(loaded->styled_source, hermes::StyledDocumentSource::kRtf);
    HERMES_CHECK_EQ(loaded->rtf_body, std::string("{\\rtf1\\ansi Rich plain}"));
    HERMES_CHECK(loaded->html_body.empty());
}

HERMES_TEST(FilesystemMessageStoreKeepsPlainSourceWhenHelperRepresentationsExist) {
    hermes::tests::ScopedTempDirectory temp("hemera-messages-helpers");
    hermes::FilesystemMessageStore store(temp.Path());

    hermes::MessageRecord message;
    message.id = "89";
    message.mailbox_id = "inbox";
    message.subject = "Plain with helpers";
    message.sender = "alice@example.com";
    message.recipients = "bob@example.com";
    message.plain_text_body = "Plain only";
    message.html_body = "<div>Plain only</div>";
    message.rtf_body = "{\\rtf1\\ansi Plain only}";
    message.styled_source = hermes::StyledDocumentSource::kPlainText;

    std::string error_message;
    HERMES_CHECK(store.SaveMessage(message, &error_message));

    const auto loaded = store.GetMessage("inbox", "89");
    HERMES_CHECK(static_cast<bool>(loaded));
    HERMES_CHECK_EQ(loaded->styled_source, hermes::StyledDocumentSource::kPlainText);
    HERMES_CHECK_EQ(loaded->plain_text_body, std::string("Plain only"));
    HERMES_CHECK_EQ(loaded->html_body, std::string("<div>Plain only</div>"));
    HERMES_CHECK_EQ(loaded->rtf_body, std::string("{\\rtf1\\ansi Plain only}"));
}

HERMES_TEST(FilesystemMessageStoreMovesAndDeletesMessages) {
    hermes::tests::ScopedTempDirectory temp("hemera-message-move");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore store(temp.Path());

    std::string error_message;
    HERMES_CHECK(
        mailbox_store.EnsureMailbox({"out", "Out", {}, "", hermes::MailboxProtocol::kLocal, "", false, true, 0},
                                    &error_message));
    HERMES_CHECK(
        mailbox_store.EnsureMailbox({"sent", "Sent", {}, "", hermes::MailboxProtocol::kLocal, "", false, true, 0},
                                    &error_message));

    hermes::MessageRecord message;
    message.id = "84";
    message.mailbox_id = "out";
    message.subject = "Move me";
    message.sender = "alice@example.com";
    message.recipients = "bob@example.com";
    message.plain_text_body = "Queued";
    HERMES_CHECK(store.SaveMessage(message, &error_message));
    HERMES_CHECK(store.MoveMessage("out", "84", "sent", &error_message));
    HERMES_CHECK(!store.GetMessage("out", "84"));
    HERMES_CHECK(static_cast<bool>(store.GetMessage("sent", "84")));
    HERMES_CHECK(store.DeleteMessage("sent", "84", &error_message));
    HERMES_CHECK(!store.GetMessage("sent", "84"));
}

HERMES_TEST(FilesystemMessageStorePersistsAttachmentPayloads) {
    hermes::tests::ScopedTempDirectory temp("hemera-message-attachments");
    hermes::FilesystemMessageStore store(temp.Path());

    const auto source_attachment = temp.Path() / "report.txt";
    {
        std::ofstream output(source_attachment);
        output << "payload";
    }

    hermes::MessageRecord message;
    message.id = "msg-attachment";
    message.mailbox_id = "out";
    message.subject = "Attachment";
    message.sender = "alice@example.com";
    message.recipients = "bob@example.com";
    message.plain_text_body = "See file";
    hermes::MessageAttachment attachment;
    attachment.name = "report.txt";
    attachment.content_type = "text/plain";
    attachment.size = 7;
    attachment.payload_path = source_attachment.string();
    message.attachments.push_back(attachment);

    std::string error_message;
    HERMES_CHECK(store.SaveMessage(message, &error_message));
    const auto loaded = store.GetMessage("out", "msg-attachment");
    HERMES_CHECK(static_cast<bool>(loaded));
    HERMES_CHECK_EQ(loaded->attachments.size(), static_cast<std::size_t>(1));
    HERMES_CHECK(!loaded->attachments.front().payload_path.empty());
    const auto payload = store.LoadAttachmentPayload("out", "msg-attachment", 0);
    HERMES_CHECK(static_cast<bool>(payload));
    HERMES_CHECK_EQ(*payload, std::string("payload"));
}
