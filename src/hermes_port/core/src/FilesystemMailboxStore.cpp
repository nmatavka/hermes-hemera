#include "hermes/MailboxStore.h"

#include "EudoraStorage.h"

namespace hermes {

FilesystemMailboxStore::FilesystemMailboxStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

bool FilesystemMailboxStore::EnsureMailbox(const MailboxRecord& mailbox, std::string* error_message) {
    return eudora::EnsureMailbox(root_directory_, mailbox, error_message);
}

std::vector<MailboxRecord> FilesystemMailboxStore::ListMailboxes() const {
    return eudora::ListMailboxes(root_directory_);
}

std::optional<MailboxRecord> FilesystemMailboxStore::GetMailbox(std::string_view mailbox_id) const {
    return eudora::GetMailbox(root_directory_, mailbox_id);
}

bool FilesystemMailboxStore::DeleteMailbox(std::string_view mailbox_id, std::string* error_message) {
    return eudora::DeleteMailbox(root_directory_, mailbox_id, error_message);
}

bool FilesystemMailboxStore::RenameMailbox(std::string_view mailbox_id,
                                           std::string_view new_mailbox_id,
                                           std::string_view new_display_name,
                                           std::string* error_message) {
    return eudora::RenameMailbox(
        root_directory_, mailbox_id, new_mailbox_id, new_display_name, error_message);
}

std::filesystem::path FilesystemMailboxStore::RootDirectory() const {
    return root_directory_;
}

std::filesystem::path FilesystemMailboxStore::MailboxesDirectory() const {
    return root_directory_;
}

}  // namespace hermes
