#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

struct MailboxSummary {
    std::string id;
    std::string display_name;
    std::size_t unread_count = 0;
};

struct MessageSummary {
    std::string id;
    std::string mailbox_id;
    std::string subject;
    std::string sender;
    std::string preview;
    bool unread = true;
};

class WorkspaceModel {
public:
    virtual ~WorkspaceModel() = default;

    virtual std::vector<MailboxSummary> Mailboxes() const = 0;
    virtual std::vector<MessageSummary> MessagesForMailbox(std::string_view mailbox_id) const = 0;
    virtual std::optional<MessageSummary> GetMessage(std::string_view message_id) const = 0;
};

}  // namespace hermes
