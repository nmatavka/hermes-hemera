#include "hermes/ComposeQueue.h"

#include <chrono>

namespace hermes {

namespace {

std::string GenerateQueuedMessageId() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    return "queued-" + std::to_string(static_cast<long long>(micros));
}

std::string WrapPlainText(std::string_view text, std::size_t width) {
    if (width < 20) {
        return std::string(text);
    }

    std::string wrapped;
    std::size_t line_length = 0;
    std::size_t word_start = 0;
    auto flush_word = [&](std::size_t end) {
        const std::string_view word = text.substr(word_start, end - word_start);
        if (word.empty()) {
            return;
        }
        if (line_length != 0 && line_length + 1 + word.size() > width) {
            wrapped.push_back('\n');
            line_length = 0;
        } else if (line_length != 0) {
            wrapped.push_back(' ');
            ++line_length;
        }
        wrapped.append(word.data(), word.size());
        line_length += word.size();
    };

    for (std::size_t index = 0; index <= text.size(); ++index) {
        const bool at_end = index == text.size();
        const char ch = at_end ? '\0' : text[index];
        if (!at_end && ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            continue;
        }
        flush_word(index);
        word_start = index + 1;
        if (at_end) {
            break;
        }
        if (ch == '\n') {
            wrapped.push_back('\n');
            line_length = 0;
        } else if (ch == '\r') {
            if (index + 1 < text.size() && text[index + 1] == '\n') {
                continue;
            }
            wrapped.push_back('\n');
            line_length = 0;
        }
    }

    return wrapped;
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
    queued.plain_text_body = message.options.word_wrap
                                 ? WrapPlainText(message.body.plain_text,
                                                 static_cast<std::size_t>(message.policy.word_wrap_max))
                                 : message.body.plain_text;
    queued.html_body = message.body.html_fragment;
    queued.account_id = message.headers.from_persona.empty() ? "primary" : message.headers.from_persona;
    queued.delivery_state = MessageDeliveryState::kQueued;
    queued.created_at = static_cast<std::int64_t>(now);
    queued.updated_at = queued.created_at;
    queued.unread = false;
    queued.compose_options = message.options;
    queued.use_legacy_return_receipt_header = message.policy.return_receipt_legacy_header;
    for (const auto& attachment : message.attachments) {
        MessageAttachment queued_attachment;
        queued_attachment.name =
            attachment.display_name.empty() ? attachment.source_path.filename().string() : attachment.display_name;
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
