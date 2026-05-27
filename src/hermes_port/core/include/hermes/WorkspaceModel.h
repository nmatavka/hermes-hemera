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
    std::size_t attachment_count = 0;
};

struct AttachmentSummary {
    std::string name;
    std::string content_type;
    std::size_t size = 0;
    bool omitted = false;
    bool download_complete = true;
    std::string fetch_error;
};

struct MessageDetail {
    std::string id;
    std::string mailbox_id;
    std::string subject;
    std::string sender;
    std::string recipients;
    std::string preview;
    std::string plain_text_body;
    bool unread = true;
    bool download_complete = true;
    bool attachments_omitted = false;
    bool flagged = false;
    bool deleted = false;
    bool answered = false;
    std::string last_error;
    std::vector<AttachmentSummary> attachments;
};

class WorkspaceModel {
public:
    virtual ~WorkspaceModel() = default;

    virtual std::vector<MailboxSummary> Mailboxes() const = 0;
    virtual std::vector<MessageSummary> MessagesForMailbox(std::string_view mailbox_id) const = 0;
    virtual std::optional<MessageSummary> GetMessage(std::string_view message_id) const = 0;
    virtual std::optional<MessageDetail> GetMessageDetail(std::string_view message_id) const = 0;
};

}  // namespace hermes
