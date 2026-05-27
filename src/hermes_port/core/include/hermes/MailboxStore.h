#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

enum class MailboxProtocol {
    kLocal,
    kPop,
    kImap,
    kSmtp,
};

struct MailboxRecord {
    std::string id;
    std::string display_name;
    std::filesystem::path path;
    std::string account_id;
    MailboxProtocol protocol = MailboxProtocol::kLocal;
    std::string remote_name;
    bool is_remote = false;
    bool system_mailbox = false;
    std::size_t message_count = 0;
};

class MailboxStore {
public:
    virtual ~MailboxStore() = default;

    virtual bool EnsureMailbox(const MailboxRecord& mailbox, std::string* error_message = nullptr) = 0;
    virtual std::vector<MailboxRecord> ListMailboxes() const = 0;
    virtual std::optional<MailboxRecord> GetMailbox(std::string_view mailbox_id) const = 0;
    virtual bool DeleteMailbox(std::string_view mailbox_id, std::string* error_message = nullptr) = 0;
    virtual bool RenameMailbox(std::string_view mailbox_id,
                               std::string_view new_mailbox_id,
                               std::string_view new_display_name = {},
                               std::string* error_message = nullptr) = 0;
};

class FilesystemMailboxStore final : public MailboxStore {
public:
    explicit FilesystemMailboxStore(std::filesystem::path root_directory);

    bool EnsureMailbox(const MailboxRecord& mailbox, std::string* error_message = nullptr) override;
    std::vector<MailboxRecord> ListMailboxes() const override;
    std::optional<MailboxRecord> GetMailbox(std::string_view mailbox_id) const override;
    bool DeleteMailbox(std::string_view mailbox_id, std::string* error_message = nullptr) override;
    bool RenameMailbox(std::string_view mailbox_id,
                       std::string_view new_mailbox_id,
                       std::string_view new_display_name = {},
                       std::string* error_message = nullptr) override;

    std::filesystem::path RootDirectory() const;

private:
    std::filesystem::path MailboxesDirectory() const;

    std::filesystem::path root_directory_;
};

}  // namespace hermes
