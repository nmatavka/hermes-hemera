#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/ComposeQueue.h"
#include "hermes/MemoryRichTextSurface.h"

HERMES_TEST(QueueComposeMessagePersistsValidatedMessagesToOutMailbox) {
    hermes::tests::ScopedTempDirectory temp("hemera-queue");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::MemoryRichTextSurface surface;

    hermes::ComposeMessage message;
    message.id = "queue-001";
    message.headers.to = "dev@example.com";
    message.headers.subject = "Queued message";
    message.headers.from_persona = "Work";
    message.body.plain_text = "Queue this message.";

    hermes::ComposeController controller(surface);
    HERMES_CHECK(controller.Load(message));

    const auto result =
        hermes::QueueComposeMessage(controller, mailbox_store, message_store, "out", false);
    HERMES_CHECK(result.queued);
    HERMES_CHECK(static_cast<bool>(result.queued_message));
    HERMES_CHECK_EQ(result.queued_message->mailbox_id, std::string("out"));

    const auto outbox = mailbox_store.GetMailbox("out");
    HERMES_CHECK(static_cast<bool>(outbox));
    HERMES_CHECK(outbox->system_mailbox);

    const auto queued_message = message_store.GetMessage("out", "queue-001");
    HERMES_CHECK(static_cast<bool>(queued_message));
    HERMES_CHECK_EQ(queued_message->subject, std::string("Queued message"));
    HERMES_CHECK_EQ(queued_message->plain_text_body, std::string("Queue this message."));
}

HERMES_TEST(QueueComposeMessageRequiresConfirmationWhenWarningsArePresent) {
    hermes::tests::ScopedTempDirectory temp("hemera-queue-warn");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::MemoryRichTextSurface surface;

    hermes::ComposeMessage message;
    message.id = "queue-002";
    message.headers.to = "team@example.com";
    message.headers.subject = "Styled queue";
    message.headers.from_persona = "Work";
    message.body.plain_text = "Styled body";
    message.body.html_fragment = "<p>Styled body</p>";
    message.body.styled_source = hermes::StyledDocumentSource::kHtml;
    message.policy.warn_on_styled_send = true;
    message.policy.send_plain_and_styled = true;

    hermes::ComposeController controller(surface);
    HERMES_CHECK(controller.Load(message));

    const auto blocked =
        hermes::QueueComposeMessage(controller, mailbox_store, message_store, "out", false);
    HERMES_CHECK(!blocked.queued);
    HERMES_CHECK(!blocked.validation.warnings.empty());

    const auto confirmed =
        hermes::QueueComposeMessage(controller, mailbox_store, message_store, "out", true);
    HERMES_CHECK(confirmed.queued);
    const auto queued_message = message_store.GetMessage("out", "queue-002");
    HERMES_CHECK(static_cast<bool>(queued_message));
    HERMES_CHECK_EQ(queued_message->styled_source, hermes::StyledDocumentSource::kHtml);
    HERMES_CHECK(!queued_message->html_body.empty());
    HERMES_CHECK(!queued_message->rtf_body.empty());
}

HERMES_TEST(QueueComposeMessageCarriesComposeOptionsAndWrapsPlainTextWhenEnabled) {
    hermes::tests::ScopedTempDirectory temp("hemera-queue-options");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::MemoryRichTextSurface surface;

    hermes::ComposeMessage message;
    message.id = "queue-options";
    message.headers.to = "dev@example.com";
    message.headers.subject = "Wrapped queue";
    message.headers.from_persona = "primary";
    message.body.plain_text = "alpha beta gamma delta epsilon zeta eta theta";
    message.policy.word_wrap_max = 24;
    message.options.priority = hermes::ComposePriority::kHighest;
    message.options.keep_copies = true;
    message.options.request_read_receipt = true;
    message.options.quoted_printable = false;
    message.options.word_wrap = true;
    message.options.tabs_in_body = false;
    message.options.text_as_document = true;

    hermes::ComposeController controller(surface);
    HERMES_CHECK(controller.Load(message));

    const auto result =
        hermes::QueueComposeMessage(controller, mailbox_store, message_store, "out", false);
    HERMES_CHECK(result.queued);

    const auto queued_message = message_store.GetMessage("out", "queue-options");
    HERMES_CHECK(static_cast<bool>(queued_message));
    HERMES_CHECK_EQ(queued_message->compose_options.priority, hermes::ComposePriority::kHighest);
    HERMES_CHECK(queued_message->compose_options.keep_copies);
    HERMES_CHECK(queued_message->compose_options.request_read_receipt);
    HERMES_CHECK(!queued_message->compose_options.quoted_printable);
    HERMES_CHECK(!queued_message->compose_options.tabs_in_body);
    HERMES_CHECK(queued_message->compose_options.text_as_document);
    HERMES_CHECK(queued_message->plain_text_body.find('\n') != std::string::npos);
}
