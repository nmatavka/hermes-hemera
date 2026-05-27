#include "hermes/InMemoryWorkspaceModel.h"

namespace hermes {

void InMemoryWorkspaceModel::AddMailbox(const MailboxSummary& mailbox) {
    mailboxes_.push_back(mailbox);
}

void InMemoryWorkspaceModel::AddMessage(const MessageSummary& message) {
    messages_.push_back(message);
}

void InMemoryWorkspaceModel::AddMessageDetail(const MessageDetail& detail) {
    message_details_.push_back(detail);
}

std::vector<MailboxSummary> InMemoryWorkspaceModel::Mailboxes() const {
    return mailboxes_;
}

std::vector<MessageSummary> InMemoryWorkspaceModel::MessagesForMailbox(std::string_view mailbox_id) const {
    std::vector<MessageSummary> matches;
    for (const auto& message : messages_) {
        if (message.mailbox_id == mailbox_id) {
            matches.push_back(message);
        }
    }
    return matches;
}

std::optional<MessageSummary> InMemoryWorkspaceModel::GetMessage(std::string_view message_id) const {
    for (const auto& message : messages_) {
        if (message.id == message_id) {
            return message;
        }
    }
    return std::nullopt;
}

std::optional<MessageDetail> InMemoryWorkspaceModel::GetMessageDetail(std::string_view message_id) const {
    for (const auto& detail : message_details_) {
        if (detail.id == message_id) {
            return detail;
        }
    }
    return std::nullopt;
}

}  // namespace hermes
