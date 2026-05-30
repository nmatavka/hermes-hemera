#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/LegacyMessageStatus.h"
#include "hermes/PopServerStatus.h"
#include "hermes/RichTextSurface.h"

namespace hermes {

struct MailboxSummary {
    std::string id;
    std::string display_name;
    std::size_t unread_count = 0;
    std::string parent_id;
    std::string account_id;
    std::string protocol;
    bool system_mailbox = false;
    bool is_remote = false;
};

struct MessageSummary {
    std::string id;
    std::string mailbox_id;
    std::string subject;
    std::string sender;
    std::string preview;
    bool unread = true;
    std::size_t attachment_count = 0;
    std::string status;
    std::string priority;
    LegacyMessageStatus legacy_status = LegacyMessageStatus::kUnread;
    int label_index = 0;
    int junk_score = 0;
    bool manually_junked = false;
    PopServerStatus pop_server_status = PopServerStatus::kNone;
    bool attachments_omitted = false;
    bool download_complete = true;
    std::size_t size = 0;
    std::int64_t timestamp = 0;
};

struct AttachmentSummary {
    std::string name;
    std::string content_type;
    std::size_t size = 0;
    bool omitted = false;
    bool download_complete = true;
    std::string fetch_error;
    std::string content_id;
    std::string disposition;
};

struct MessageDetail {
    std::string id;
    std::string mailbox_id;
    std::string subject;
    std::string sender;
    std::string recipients;
    std::string preview;
    std::string plain_text_body;
    std::string html_body;
    std::string rtf_body;
    std::string paige_native_body;
    StyledDocumentSource styled_source = StyledDocumentSource::kPlainText;
    StyledDocumentFidelity styled_fidelity = StyledDocumentFidelity::kLossless;
    bool unread = true;
    bool download_complete = true;
    bool attachments_omitted = false;
    bool flagged = false;
    bool deleted = false;
    bool answered = false;
    LegacyMessageStatus legacy_status = LegacyMessageStatus::kUnread;
    int label_index = 0;
    int junk_score = 0;
    bool manually_junked = false;
    PopServerStatus pop_server_status = PopServerStatus::kNone;
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
