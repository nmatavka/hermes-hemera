#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

struct MessageRecord {
    std::string id;
    std::string mailbox_id;
    std::string subject;
    std::string sender;
    std::string recipients;
    std::string plain_text_body;
    std::string html_body;
    bool unread = true;
};

class MessageStore {
public:
    virtual ~MessageStore() = default;

    virtual bool SaveMessage(const MessageRecord& message, std::string* error_message = nullptr) = 0;
    virtual std::vector<MessageRecord> ListMessages(std::string_view mailbox_id) const = 0;
    virtual std::optional<MessageRecord> GetMessage(std::string_view mailbox_id, std::string_view message_id) const = 0;
};

class FilesystemMessageStore final : public MessageStore {
public:
    explicit FilesystemMessageStore(std::filesystem::path root_directory);

    bool SaveMessage(const MessageRecord& message, std::string* error_message = nullptr) override;
    std::vector<MessageRecord> ListMessages(std::string_view mailbox_id) const override;
    std::optional<MessageRecord> GetMessage(std::string_view mailbox_id, std::string_view message_id) const override;

private:
    std::filesystem::path MailboxDirectory(std::string_view mailbox_id) const;
    std::filesystem::path MessagePath(std::string_view mailbox_id, std::string_view message_id) const;
    static std::optional<MessageRecord> ReadMessageFile(const std::filesystem::path& path, std::string_view mailbox_id);

    std::filesystem::path root_directory_;
};

}  // namespace hermes
