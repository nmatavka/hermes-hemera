#include "hermes/ImapActionStore.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "hermes/IniSettingsStore.h"

namespace hermes {

namespace {

std::string ActionKindToString(ImapActionKind kind) {
    switch (kind) {
        case ImapActionKind::kDelete:
            return "delete";
        case ImapActionKind::kUndelete:
            return "undelete";
        case ImapActionKind::kMove:
            return "move";
        case ImapActionKind::kCopy:
            return "copy";
        case ImapActionKind::kCreateMailbox:
            return "create-mailbox";
        case ImapActionKind::kRenameMailbox:
            return "rename-mailbox";
        case ImapActionKind::kDeleteMailbox:
            return "delete-mailbox";
        case ImapActionKind::kFetchAttachment:
            return "fetch-attachment";
        case ImapActionKind::kFetchFullMessage:
            return "fetch-full-message";
        case ImapActionKind::kResyncMailbox:
            return "resync-mailbox";
        case ImapActionKind::kRefreshMailboxList:
            return "refresh-mailbox-list";
    }
    return "refresh-mailbox-list";
}

ImapActionKind ActionKindFromString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "delete") {
        return ImapActionKind::kDelete;
    }
    if (value == "undelete") {
        return ImapActionKind::kUndelete;
    }
    if (value == "move") {
        return ImapActionKind::kMove;
    }
    if (value == "copy") {
        return ImapActionKind::kCopy;
    }
    if (value == "create-mailbox") {
        return ImapActionKind::kCreateMailbox;
    }
    if (value == "rename-mailbox") {
        return ImapActionKind::kRenameMailbox;
    }
    if (value == "delete-mailbox") {
        return ImapActionKind::kDeleteMailbox;
    }
    if (value == "fetch-attachment") {
        return ImapActionKind::kFetchAttachment;
    }
    if (value == "fetch-full-message") {
        return ImapActionKind::kFetchFullMessage;
    }
    if (value == "resync-mailbox") {
        return ImapActionKind::kResyncMailbox;
    }
    return ImapActionKind::kRefreshMailboxList;
}

std::string ActionStateToString(ImapActionState state) {
    switch (state) {
        case ImapActionState::kPending:
            return "pending";
        case ImapActionState::kRunning:
            return "running";
        case ImapActionState::kFailed:
            return "failed";
        case ImapActionState::kCompleted:
            return "completed";
        case ImapActionState::kCancelled:
            return "cancelled";
    }
    return "pending";
}

ImapActionState ActionStateFromString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "running") {
        return ImapActionState::kRunning;
    }
    if (value == "failed") {
        return ImapActionState::kFailed;
    }
    if (value == "completed") {
        return ImapActionState::kCompleted;
    }
    if (value == "cancelled") {
        return ImapActionState::kCancelled;
    }
    return ImapActionState::kPending;
}

}  // namespace

FilesystemImapActionStore::FilesystemImapActionStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

bool FilesystemImapActionStore::SaveAction(const ImapActionRecord& action, std::string* error_message) {
    if (action.id.empty()) {
        if (error_message) {
            *error_message = "IMAP action id must not be empty.";
        }
        return false;
    }

    std::error_code create_error;
    std::filesystem::create_directories(ActionsDirectory(), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create IMAP action directory: " + create_error.message();
        }
        return false;
    }

    IniSettingsStore settings;
    settings.SetString("Action", "Id", action.id);
    settings.SetString("Action", "Kind", ActionKindToString(action.kind));
    settings.SetString("Action", "State", ActionStateToString(action.state));
    settings.SetString("Action", "AccountId", action.account_id);
    settings.SetString("Action", "MailboxId", action.mailbox_id);
    settings.SetString("Action", "RemoteMailbox", action.remote_mailbox);
    settings.SetString("Action", "MessageId", action.message_id);
    settings.SetString("Action", "RemoteMessageId", action.remote_message_id);
    settings.SetString("Action", "DestinationMailboxId", action.destination_mailbox_id);
    settings.SetString("Action", "DestinationRemoteMailbox", action.destination_remote_mailbox);
    settings.SetString("Action", "RenameTarget", action.rename_target);
    settings.SetString("Action", "AttachmentIndex", std::to_string(action.attachment_index));
    settings.SetString("Action", "CreatedAt", std::to_string(action.created_at));
    settings.SetString("Action", "UpdatedAt", std::to_string(action.updated_at));
    settings.SetString("Action", "Attempts", std::to_string(action.attempts));
    settings.SetString("Action", "LastError", action.last_error);
    return settings.SaveToFile(ActionPath(action.id), error_message);
}

std::vector<ImapActionRecord> FilesystemImapActionStore::ListActions() const {
    std::vector<ImapActionRecord> actions;
    if (!std::filesystem::exists(ActionsDirectory())) {
        return actions;
    }

    for (const auto& entry : std::filesystem::directory_iterator(ActionsDirectory())) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ini") {
            continue;
        }
        const auto action = GetAction(entry.path().stem().string());
        if (action) {
            actions.push_back(*action);
        }
    }

    std::sort(actions.begin(), actions.end(), [](const ImapActionRecord& left, const ImapActionRecord& right) {
        if (left.created_at != right.created_at) {
            return left.created_at < right.created_at;
        }
        return left.id < right.id;
    });
    return actions;
}

std::optional<ImapActionRecord> FilesystemImapActionStore::GetAction(std::string_view action_id) const {
    IniSettingsStore settings;
    std::string ignored;
    if (!settings.LoadFromFile(ActionPath(action_id), &ignored)) {
        return std::nullopt;
    }

    ImapActionRecord action;
    action.id = settings.GetString("Action", "Id").value_or(std::string(action_id));
    action.kind = ActionKindFromString(settings.GetString("Action", "Kind").value_or(""));
    action.state = ActionStateFromString(settings.GetString("Action", "State").value_or(""));
    action.account_id = settings.GetString("Action", "AccountId").value_or("");
    action.mailbox_id = settings.GetString("Action", "MailboxId").value_or("");
    action.remote_mailbox = settings.GetString("Action", "RemoteMailbox").value_or("");
    action.message_id = settings.GetString("Action", "MessageId").value_or("");
    action.remote_message_id = settings.GetString("Action", "RemoteMessageId").value_or("");
    action.destination_mailbox_id =
        settings.GetString("Action", "DestinationMailboxId").value_or("");
    action.destination_remote_mailbox =
        settings.GetString("Action", "DestinationRemoteMailbox").value_or("");
    action.rename_target = settings.GetString("Action", "RenameTarget").value_or("");
    action.attachment_index = static_cast<std::size_t>(
        std::max(settings.GetInt("Action", "AttachmentIndex", 0), 0));
    action.created_at = static_cast<std::int64_t>(settings.GetInt("Action", "CreatedAt", 0));
    action.updated_at = static_cast<std::int64_t>(settings.GetInt("Action", "UpdatedAt", 0));
    action.attempts = settings.GetInt("Action", "Attempts", 0);
    action.last_error = settings.GetString("Action", "LastError").value_or("");
    return action;
}

bool FilesystemImapActionStore::DeleteAction(std::string_view action_id, std::string* error_message) {
    std::error_code remove_error;
    std::filesystem::remove(ActionPath(action_id), remove_error);
    if (remove_error && error_message) {
        *error_message = "Unable to delete IMAP action: " + remove_error.message();
    }
    return !remove_error;
}

std::filesystem::path FilesystemImapActionStore::ActionsDirectory() const {
    return root_directory_ / "imap-actions";
}

std::filesystem::path FilesystemImapActionStore::ActionPath(std::string_view action_id) const {
    return ActionsDirectory() / (std::string(action_id) + ".ini");
}

}  // namespace hermes
