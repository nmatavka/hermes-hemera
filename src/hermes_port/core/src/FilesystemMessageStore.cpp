#include "hermes/MessageStore.h"

#include "EudoraStorage.h"

namespace hermes {

FilesystemMessageStore::FilesystemMessageStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

bool FilesystemMessageStore::SaveMessage(const MessageRecord& message, std::string* error_message) {
    return eudora::SaveMessage(root_directory_, message, error_message);
}

std::vector<MessageRecord> FilesystemMessageStore::ListMessages(std::string_view mailbox_id) const {
    return eudora::ListMessages(root_directory_, mailbox_id);
}

std::optional<MessageRecord> FilesystemMessageStore::GetMessage(std::string_view mailbox_id,
                                                                std::string_view message_id) const {
    return eudora::GetMessage(root_directory_, mailbox_id, message_id);
}

bool FilesystemMessageStore::DeleteMessage(std::string_view mailbox_id,
                                           std::string_view message_id,
                                           std::string* error_message) {
    return eudora::DeleteMessage(root_directory_, mailbox_id, message_id, error_message);
}

bool FilesystemMessageStore::CopyMessage(std::string_view source_mailbox_id,
                                         std::string_view message_id,
                                         std::string_view destination_mailbox_id,
                                         std::string* error_message) {
    return eudora::CopyMessage(
        root_directory_, source_mailbox_id, message_id, destination_mailbox_id, error_message);
}

bool FilesystemMessageStore::MoveMessage(std::string_view source_mailbox_id,
                                         std::string_view message_id,
                                         std::string_view destination_mailbox_id,
                                         std::string* error_message) {
    return eudora::MoveMessage(
        root_directory_, source_mailbox_id, message_id, destination_mailbox_id, error_message);
}

bool FilesystemMessageStore::SaveAttachmentPayload(std::string_view mailbox_id,
                                                   std::string_view message_id,
                                                   std::size_t attachment_index,
                                                   std::string_view suggested_name,
                                                   std::string_view payload,
                                                   std::string* error_message) {
    return eudora::SaveAttachmentPayload(root_directory_,
                                         mailbox_id,
                                         message_id,
                                         attachment_index,
                                         suggested_name,
                                         payload,
                                         error_message);
}

bool FilesystemMessageStore::ImportAttachmentFile(std::string_view mailbox_id,
                                                  std::string_view message_id,
                                                  std::size_t attachment_index,
                                                  const std::filesystem::path& source_path,
                                                  std::string* error_message) {
    return eudora::ImportAttachmentFile(
        root_directory_, mailbox_id, message_id, attachment_index, source_path, error_message);
}

std::optional<std::string> FilesystemMessageStore::LoadAttachmentPayload(
    std::string_view mailbox_id,
    std::string_view message_id,
    std::size_t attachment_index) const {
    return eudora::LoadAttachmentPayload(root_directory_, mailbox_id, message_id, attachment_index);
}

std::optional<std::filesystem::path> FilesystemMessageStore::AttachmentPath(
    std::string_view mailbox_id,
    std::string_view message_id,
    std::size_t attachment_index) const {
    return eudora::AttachmentPath(root_directory_, mailbox_id, message_id, attachment_index);
}

std::filesystem::path FilesystemMessageStore::MailboxDirectory(std::string_view mailbox_id) const {
    return root_directory_ / std::string(mailbox_id);
}

std::filesystem::path FilesystemMessageStore::MessagePath(std::string_view mailbox_id,
                                                          std::string_view message_id) const {
    return MailboxDirectory(mailbox_id) / (std::string(message_id) + ".mbx");
}

std::filesystem::path FilesystemMessageStore::AttachmentsDirectory(std::string_view message_id) const {
    return root_directory_ / "Attach" / std::string(message_id);
}

std::optional<MessageRecord> FilesystemMessageStore::ReadMessageFile(const std::filesystem::path&,
                                                                     std::string_view) {
    return std::nullopt;
}

}  // namespace hermes
