#include "hermes/SyncStateStore.h"

#include <filesystem>

#include "hermes/IniSettingsStore.h"

namespace hermes {

namespace {

std::string Sanitize(std::string_view value) {
    std::string sanitized;
    sanitized.reserve(value.size());
    for (char ch : value) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            sanitized.push_back(ch);
        } else {
            sanitized.push_back('_');
        }
    }
    return sanitized;
}

std::filesystem::path PopStatePath(const std::filesystem::path& root, std::string_view account_id) {
    return root / ("pop-" + Sanitize(account_id) + ".ini");
}

std::filesystem::path ImapStatePath(const std::filesystem::path& root,
                                    std::string_view account_id,
                                    std::string_view mailbox_id) {
    return root / ("imap-" + Sanitize(account_id) + "-" + Sanitize(mailbox_id) + ".ini");
}

}  // namespace

FilesystemSyncStateStore::FilesystemSyncStateStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

std::optional<PopSyncState> FilesystemSyncStateStore::LoadPopState(std::string_view account_id) const {
    const auto path = PopStatePath(SyncDirectory(), account_id);
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    IniSettingsStore settings;
    std::string ignored;
    if (!settings.LoadFromFile(path, &ignored)) {
        return std::nullopt;
    }

    PopSyncState state;
    state.account_id = std::string(account_id);
    for (const auto& section : settings.Sections()) {
        if (section != "UIDL") {
            continue;
        }
        for (int index = 0;; ++index) {
            const auto uidl = settings.GetString("UIDL", "Uidl" + std::to_string(index));
            const auto message_id = settings.GetString("UIDL", "Message" + std::to_string(index));
            if (!uidl || !message_id) {
                break;
            }
            state.uidl_to_message_id[*uidl] = *message_id;
        }
    }
    for (const auto& section : settings.Sections()) {
        if (section != "ServerStatus") {
            continue;
        }
        for (int index = 0;; ++index) {
            const auto uidl = settings.GetString("ServerStatus", "Uidl" + std::to_string(index));
            const auto status = settings.GetString("ServerStatus", "Status" + std::to_string(index));
            if (!uidl || !status) {
                break;
            }
            state.uidl_to_server_status[*uidl] = PopServerStatusFromString(*status);
        }
    }
    return state;
}

bool FilesystemSyncStateStore::SavePopState(const PopSyncState& state, std::string* error_message) {
    std::error_code create_error;
    std::filesystem::create_directories(SyncDirectory(), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create sync directory: " + create_error.message();
        }
        return false;
    }

    IniSettingsStore settings;
    int index = 0;
    for (const auto& entry : state.uidl_to_message_id) {
        settings.SetString("UIDL", "Uidl" + std::to_string(index), entry.first);
        settings.SetString("UIDL", "Message" + std::to_string(index), entry.second);
        ++index;
    }
    index = 0;
    for (const auto& entry : state.uidl_to_server_status) {
        settings.SetString("ServerStatus", "Uidl" + std::to_string(index), entry.first);
        settings.SetString("ServerStatus", "Status" + std::to_string(index), ToString(entry.second));
        ++index;
    }
    return settings.SaveToFile(PopStatePath(SyncDirectory(), state.account_id), error_message);
}

std::optional<ImapMailboxSyncState> FilesystemSyncStateStore::LoadImapState(std::string_view account_id,
                                                                             std::string_view mailbox_id) const {
    const auto path = ImapStatePath(SyncDirectory(), account_id, mailbox_id);
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    IniSettingsStore settings;
    std::string ignored;
    if (!settings.LoadFromFile(path, &ignored)) {
        return std::nullopt;
    }

    ImapMailboxSyncState state;
    state.account_id = std::string(account_id);
    state.mailbox_id = std::string(mailbox_id);
    state.uid_validity = static_cast<std::uint64_t>(settings.GetInt("State", "UidValidity", 0));
    state.last_seen_uid = static_cast<std::uint64_t>(settings.GetInt("State", "LastSeenUid", 0));
    state.auto_sync = settings.GetBool("State", "AutoSync", true);
    state.show_deleted = settings.GetBool("State", "ShowDeleted", false);
    return state;
}

bool FilesystemSyncStateStore::SaveImapState(const ImapMailboxSyncState& state,
                                             std::string* error_message) {
    std::error_code create_error;
    std::filesystem::create_directories(SyncDirectory(), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create sync directory: " + create_error.message();
        }
        return false;
    }

    IniSettingsStore settings;
    settings.SetString("State", "UidValidity", std::to_string(state.uid_validity));
    settings.SetString("State", "LastSeenUid", std::to_string(state.last_seen_uid));
    settings.SetString("State", "AutoSync", state.auto_sync ? "1" : "0");
    settings.SetString("State", "ShowDeleted", state.show_deleted ? "1" : "0");
    return settings.SaveToFile(ImapStatePath(SyncDirectory(), state.account_id, state.mailbox_id), error_message);
}

std::filesystem::path FilesystemSyncStateStore::SyncDirectory() const {
    return root_directory_ / "sync";
}

}  // namespace hermes
