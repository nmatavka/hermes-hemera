#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace hermes {

struct PopSyncState {
    std::string account_id;
    std::map<std::string, std::string> uidl_to_message_id;
};

struct ImapMailboxSyncState {
    std::string account_id;
    std::string mailbox_id;
    std::uint64_t uid_validity = 0;
    std::uint64_t last_seen_uid = 0;
};

class SyncStateStore {
public:
    virtual ~SyncStateStore() = default;

    virtual std::optional<PopSyncState> LoadPopState(std::string_view account_id) const = 0;
    virtual bool SavePopState(const PopSyncState& state, std::string* error_message = nullptr) = 0;
    virtual std::optional<ImapMailboxSyncState> LoadImapState(std::string_view account_id,
                                                              std::string_view mailbox_id) const = 0;
    virtual bool SaveImapState(const ImapMailboxSyncState& state,
                               std::string* error_message = nullptr) = 0;
};

class FilesystemSyncStateStore final : public SyncStateStore {
public:
    explicit FilesystemSyncStateStore(std::filesystem::path root_directory);

    std::optional<PopSyncState> LoadPopState(std::string_view account_id) const override;
    bool SavePopState(const PopSyncState& state, std::string* error_message = nullptr) override;
    std::optional<ImapMailboxSyncState> LoadImapState(std::string_view account_id,
                                                      std::string_view mailbox_id) const override;
    bool SaveImapState(const ImapMailboxSyncState& state,
                       std::string* error_message = nullptr) override;

private:
    std::filesystem::path SyncDirectory() const;

    std::filesystem::path root_directory_;
};

}  // namespace hermes
