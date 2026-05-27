#pragma once

#include <vector>

#include "hermes/WorkspaceModel.h"

namespace hermes {

class InMemoryWorkspaceModel final : public WorkspaceModel {
public:
    void AddMailbox(const MailboxSummary& mailbox);
    void AddMessage(const MessageSummary& message);
    void AddMessageDetail(const MessageDetail& detail);

    std::vector<MailboxSummary> Mailboxes() const override;
    std::vector<MessageSummary> MessagesForMailbox(std::string_view mailbox_id) const override;
    std::optional<MessageSummary> GetMessage(std::string_view message_id) const override;
    std::optional<MessageDetail> GetMessageDetail(std::string_view message_id) const override;

private:
    std::vector<MailboxSummary> mailboxes_;
    std::vector<MessageSummary> messages_;
    std::vector<MessageDetail> message_details_;
};

}  // namespace hermes
