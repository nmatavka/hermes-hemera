#include "hermes/ComposeQueue.h"

#include <chrono>

namespace hermes {

namespace {

std::string GenerateQueuedMessageId() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    return "queued-" + std::to_string(static_cast<long long>(micros));
}

MessageRecord BuildQueuedRecord(const ComposeMessage& message, std::string_view mailbox_id) {
    MessageRecord queued;
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    queued.id = message.id.empty() ? GenerateQueuedMessageId() : message.id;
    queued.mailbox_id = std::string(mailbox_id);
    queued.subject = message.headers.subject;
    queued.sender = message.headers.from_persona;
    queued.recipients = message.headers.to;
    if (!message.headers.cc.empty()) {
        if (!queued.recipients.empty()) {
            queued.recipients += ", ";
        }
        queued.recipients += message.headers.cc;
    }
    if (!message.headers.bcc.empty()) {
        if (!queued.recipients.empty()) {
            queued.recipients += ", ";
        }
        queued.recipients += message.headers.bcc;
    }
    queued.plain_text_body = message.body.plain_text;
    queued.html_body = message.body.html_fragment;
    queued.account_id = message.headers.from_persona.empty() ? "primary" : message.headers.from_persona;
    queued.delivery_state = MessageDeliveryState::kQueued;
    queued.created_at = static_cast<std::int64_t>(now);
    queued.updated_at = queued.created_at;
    queued.unread = false;
    for (const auto& attachment : message.attachments) {
        MessageAttachment queued_attachment;
        queued_attachment.name = attachment.display_name;
        queued_attachment.content_type = attachment.mime_type;
        queued_attachment.size = static_cast<std::size_t>(attachment.size);
        queued_attachment.payload_path = attachment.source_path.string();
        queued_attachment.content_id = attachment.content_id;
        queued_attachment.disposition = attachment.inline_disposition ? "inline" : "attachment";
        queued_attachment.download_complete = true;
        queued.attachments.push_back(std::move(queued_attachment));
    }
    return queued;
}

}  // namespace

QueueComposeResult QueueComposeMessage(ComposeController& controller,
                                       MailboxStore& mailbox_store,
                                       MessageStore& message_store,
                                       std::string_view mailbox_id,
                                       bool allow_warnings) {
    QueueComposeResult result;
    result.validation = controller.ValidateForSend();
    if (!result.validation.blocking_errors.empty()) {
        return result;
    }

    if (!allow_warnings && !result.validation.warnings.empty()) {
        return result;
    }

    MailboxRecord mailbox;
    mailbox.id = std::string(mailbox_id);
    mailbox.display_name = mailbox.id == "out" ? "Out" : mailbox.id;
    mailbox.protocol = MailboxProtocol::kLocal;
    mailbox.system_mailbox = mailbox.id == "out";
    if (!mailbox_store.EnsureMailbox(mailbox, &result.error_message)) {
        return result;
    }

    const ComposeMessage snapshot = controller.Snapshot();
    MessageRecord queued = BuildQueuedRecord(snapshot, mailbox_id);
    if (!message_store.SaveMessage(queued, &result.error_message)) {
        return result;
    }

    result.queued = true;
    result.queued_message = queued;
    return result;
}

}  // namespace hermes
