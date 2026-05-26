#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

struct MailboxRecord {
    std::string id;
    std::string display_name;
    std::filesystem::path path;
    bool system_mailbox = false;
    std::size_t message_count = 0;
};

class MailboxStore {
public:
    virtual ~MailboxStore() = default;

    virtual bool EnsureMailbox(const MailboxRecord& mailbox, std::string* error_message = nullptr) = 0;
    virtual std::vector<MailboxRecord> ListMailboxes() const = 0;
    virtual std::optional<MailboxRecord> GetMailbox(std::string_view mailbox_id) const = 0;
};

class FilesystemMailboxStore final : public MailboxStore {
public:
    explicit FilesystemMailboxStore(std::filesystem::path root_directory);

    bool EnsureMailbox(const MailboxRecord& mailbox, std::string* error_message = nullptr) override;
    std::vector<MailboxRecord> ListMailboxes() const override;
    std::optional<MailboxRecord> GetMailbox(std::string_view mailbox_id) const override;

    std::filesystem::path RootDirectory() const;

private:
    std::filesystem::path MailboxesDirectory() const;

    std::filesystem::path root_directory_;
};

}  // namespace hermes
