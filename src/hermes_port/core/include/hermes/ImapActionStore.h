#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

enum class ImapActionKind {
    kDelete,
    kUndelete,
    kExpungeMailbox,
    kMove,
    kCopy,
    kCreateMailbox,
    kRenameMailbox,
    kDeleteMailbox,
    kFetchAttachment,
    kFetchDefaultMessage,
    kFetchFullMessage,
    kRedownloadDefaultMessage,
    kRedownloadFullMessage,
    kResyncMailbox,
    kRefreshMailboxList,
};

enum class ImapActionState {
    kPending,
    kRunning,
    kFailed,
    kCompleted,
    kCancelled,
};

struct ImapActionRecord {
    std::string id;
    ImapActionKind kind = ImapActionKind::kRefreshMailboxList;
    ImapActionState state = ImapActionState::kPending;
    std::string account_id;
    std::string mailbox_id;
    std::string remote_mailbox;
    std::string message_id;
    std::string remote_message_id;
    std::string destination_mailbox_id;
    std::string destination_remote_mailbox;
    std::string rename_target;
    std::size_t attachment_index = 0;
    std::int64_t created_at = 0;
    std::int64_t updated_at = 0;
    int attempts = 0;
    std::string last_error;
};

class ImapActionStore {
public:
    virtual ~ImapActionStore() = default;

    virtual bool SaveAction(const ImapActionRecord& action, std::string* error_message = nullptr) = 0;
    virtual std::vector<ImapActionRecord> ListActions() const = 0;
    virtual std::optional<ImapActionRecord> GetAction(std::string_view action_id) const = 0;
    virtual bool DeleteAction(std::string_view action_id, std::string* error_message = nullptr) = 0;
};

class FilesystemImapActionStore final : public ImapActionStore {
public:
    explicit FilesystemImapActionStore(std::filesystem::path root_directory);

    bool SaveAction(const ImapActionRecord& action, std::string* error_message = nullptr) override;
    std::vector<ImapActionRecord> ListActions() const override;
    std::optional<ImapActionRecord> GetAction(std::string_view action_id) const override;
    bool DeleteAction(std::string_view action_id, std::string* error_message = nullptr) override;

private:
    std::filesystem::path ActionsDirectory() const;
    std::filesystem::path ActionPath(std::string_view action_id) const;

    std::filesystem::path root_directory_;
};

}  // namespace hermes
