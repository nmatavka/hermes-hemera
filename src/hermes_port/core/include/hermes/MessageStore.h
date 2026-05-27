#pragma once

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

enum class MessageDeliveryState {
    kDraft,
    kQueued,
    kSending,
    kSent,
    kReceived,
    kFailed,
};

struct MessageAttachment {
    std::string name;
    std::string content_type;
    std::size_t size = 0;
    bool omitted = false;
};

struct MessageRecord {
    std::string id;
    std::string mailbox_id;
    std::string account_id;
    std::string subject;
    std::string sender;
    std::string recipients;
    std::string plain_text_body;
    std::string html_body;
    MessageDeliveryState delivery_state = MessageDeliveryState::kReceived;
    std::string remote_id;
    std::string remote_mailbox;
    bool download_complete = true;
    bool attachments_omitted = false;
    bool flagged = false;
    bool deleted = false;
    bool answered = false;
    std::string last_error;
    std::int64_t created_at = 0;
    std::int64_t updated_at = 0;
    bool unread = true;
    std::vector<MessageAttachment> attachments;
};

class MessageStore {
public:
    virtual ~MessageStore() = default;

    virtual bool SaveMessage(const MessageRecord& message, std::string* error_message = nullptr) = 0;
    virtual std::vector<MessageRecord> ListMessages(std::string_view mailbox_id) const = 0;
    virtual std::optional<MessageRecord> GetMessage(std::string_view mailbox_id, std::string_view message_id) const = 0;
    virtual bool DeleteMessage(std::string_view mailbox_id,
                               std::string_view message_id,
                               std::string* error_message = nullptr) = 0;
    virtual bool MoveMessage(std::string_view source_mailbox_id,
                             std::string_view message_id,
                             std::string_view destination_mailbox_id,
                             std::string* error_message = nullptr) = 0;
};

class FilesystemMessageStore final : public MessageStore {
public:
    explicit FilesystemMessageStore(std::filesystem::path root_directory);

    bool SaveMessage(const MessageRecord& message, std::string* error_message = nullptr) override;
    std::vector<MessageRecord> ListMessages(std::string_view mailbox_id) const override;
    std::optional<MessageRecord> GetMessage(std::string_view mailbox_id, std::string_view message_id) const override;
    bool DeleteMessage(std::string_view mailbox_id,
                       std::string_view message_id,
                       std::string* error_message = nullptr) override;
    bool MoveMessage(std::string_view source_mailbox_id,
                     std::string_view message_id,
                     std::string_view destination_mailbox_id,
                     std::string* error_message = nullptr) override;

private:
    std::filesystem::path MailboxDirectory(std::string_view mailbox_id) const;
    std::filesystem::path MessagePath(std::string_view mailbox_id, std::string_view message_id) const;
    static std::optional<MessageRecord> ReadMessageFile(const std::filesystem::path& path, std::string_view mailbox_id);

    std::filesystem::path root_directory_;
};

}  // namespace hermes
