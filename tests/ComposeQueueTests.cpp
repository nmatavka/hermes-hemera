#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/ComposeQueue.h"
#include "hermes/MemoryRichTextSurface.h"

HERMES_TEST(QueueComposeMessagePersistsValidatedMessagesToOutMailbox) {
    hermes::tests::ScopedTempDirectory temp("hermes-queue");
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
    hermes::tests::ScopedTempDirectory temp("hermes-queue-warn");
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
}
