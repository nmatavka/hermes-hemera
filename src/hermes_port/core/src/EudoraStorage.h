#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/ComposeMessage.h"
#include "hermes/DraftStore.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"

namespace hermes::eudora {

bool EnsureCanonicalStoreReady(const std::filesystem::path& root_directory,
                               std::string* error_message = nullptr);

bool EnsureMailbox(const std::filesystem::path& root_directory,
                   const MailboxRecord& mailbox,
                   std::string* error_message = nullptr);
std::vector<MailboxRecord> ListMailboxes(const std::filesystem::path& root_directory);
std::optional<MailboxRecord> GetMailbox(const std::filesystem::path& root_directory,
                                        std::string_view mailbox_id);
bool DeleteMailbox(const std::filesystem::path& root_directory,
                   std::string_view mailbox_id,
                   std::string* error_message = nullptr);
bool RenameMailbox(const std::filesystem::path& root_directory,
                   std::string_view mailbox_id,
                   std::string_view new_mailbox_id,
                   std::string_view new_display_name = {},
                   std::string* error_message = nullptr);

bool SaveMessage(const std::filesystem::path& root_directory,
                 const MessageRecord& message,
                 std::string* error_message = nullptr);
std::vector<MessageRecord> ListMessages(const std::filesystem::path& root_directory,
                                        std::string_view mailbox_id);
std::optional<MessageRecord> GetMessage(const std::filesystem::path& root_directory,
                                        std::string_view mailbox_id,
                                        std::string_view message_id);
bool DeleteMessage(const std::filesystem::path& root_directory,
                   std::string_view mailbox_id,
                   std::string_view message_id,
                   std::string* error_message = nullptr);
bool CopyMessage(const std::filesystem::path& root_directory,
                 std::string_view source_mailbox_id,
                 std::string_view message_id,
                 std::string_view destination_mailbox_id,
                 std::string* error_message = nullptr);
bool MoveMessage(const std::filesystem::path& root_directory,
                 std::string_view source_mailbox_id,
                 std::string_view message_id,
                 std::string_view destination_mailbox_id,
                 std::string* error_message = nullptr);
bool CompactMailbox(const std::filesystem::path& root_directory,
                    std::string_view mailbox_id,
                    std::string* error_message = nullptr);
bool CompactAllMailboxes(const std::filesystem::path& root_directory,
                         std::string* error_message = nullptr);
bool SaveAttachmentPayload(const std::filesystem::path& root_directory,
                           std::string_view mailbox_id,
                           std::string_view message_id,
                           std::size_t attachment_index,
                           std::string_view suggested_name,
                           std::string_view payload,
                           std::string* error_message = nullptr);
bool ImportAttachmentFile(const std::filesystem::path& root_directory,
                          std::string_view mailbox_id,
                          std::string_view message_id,
                          std::size_t attachment_index,
                          const std::filesystem::path& source_path,
                          std::string* error_message = nullptr);
std::optional<std::string> LoadAttachmentPayload(const std::filesystem::path& root_directory,
                                                 std::string_view mailbox_id,
                                                 std::string_view message_id,
                                                 std::size_t attachment_index);
std::optional<std::filesystem::path> AttachmentPath(const std::filesystem::path& root_directory,
                                                    std::string_view mailbox_id,
                                                    std::string_view message_id,
                                                    std::size_t attachment_index);

bool SaveDraft(const std::filesystem::path& root_directory,
               const ComposeMessage& draft,
               std::string* error_message = nullptr);
std::optional<ComposeMessage> GetDraft(const std::filesystem::path& root_directory,
                                       std::string_view draft_id);
std::vector<ComposeMessage> ListDrafts(const std::filesystem::path& root_directory);

}  // namespace hermes::eudora
