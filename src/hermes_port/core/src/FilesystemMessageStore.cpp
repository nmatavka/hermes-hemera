#include "hermes/MessageStore.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace hermes {

FilesystemMessageStore::FilesystemMessageStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

namespace {

constexpr const char* kHtmlMarker = "\n--HERMES-HTML-BODY--\n";

std::string TrimRight(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

std::string HeaderValue(std::string_view line) {
    const std::size_t separator = line.find(':');
    if (separator == std::string::npos) {
        return {};
    }

    std::size_t start = separator + 1;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) {
        ++start;
    }
    return std::string(line.substr(start));
}

std::string DeliveryStateToString(MessageDeliveryState state) {
    switch (state) {
        case MessageDeliveryState::kDraft:
            return "draft";
        case MessageDeliveryState::kQueued:
            return "queued";
        case MessageDeliveryState::kSending:
            return "sending";
        case MessageDeliveryState::kSent:
            return "sent";
        case MessageDeliveryState::kReceived:
            return "received";
        case MessageDeliveryState::kFailed:
            return "failed";
    }
    return "received";
}

MessageDeliveryState DeliveryStateFromString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "draft") {
        return MessageDeliveryState::kDraft;
    }
    if (value == "queued") {
        return MessageDeliveryState::kQueued;
    }
    if (value == "sending") {
        return MessageDeliveryState::kSending;
    }
    if (value == "sent") {
        return MessageDeliveryState::kSent;
    }
    if (value == "failed") {
        return MessageDeliveryState::kFailed;
    }
    return MessageDeliveryState::kReceived;
}

std::string SanitizeFilename(std::string value) {
    if (value.empty()) {
        return "attachment.bin";
    }
    for (char& ch : value) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (!(std::isalnum(uch) || ch == '.' || ch == '_' || ch == '-')) {
            ch = '_';
        }
    }
    return value;
}

std::filesystem::path AttachmentStoragePath(const std::filesystem::path& root,
                                            std::string_view message_id,
                                            std::size_t attachment_index,
                                            std::string_view suggested_name) {
    return root / "Attachments" / std::string(message_id) /
           (std::to_string(attachment_index) + "-" + SanitizeFilename(std::string(suggested_name)));
}

std::optional<std::filesystem::path> ExistingAttachmentPath(const std::filesystem::path& root,
                                                            std::string_view message_id,
                                                            std::size_t attachment_index) {
    const std::filesystem::path directory = root / "Attachments" / std::string(message_id);
    if (!std::filesystem::exists(directory)) {
        return std::nullopt;
    }
    const std::string prefix = std::to_string(attachment_index) + "-";
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string filename = entry.path().filename().string();
        if (filename.rfind(prefix, 0) == 0) {
            return entry.path();
        }
    }
    return std::nullopt;
}

std::string SerializeAttachment(const MessageAttachment& attachment) {
    return attachment.name + '\t' + attachment.content_type + '\t' +
           std::to_string(attachment.size) + '\t' + (attachment.omitted ? "1" : "0") + '\t' +
           attachment.payload_path + '\t' + attachment.content_id + '\t' + attachment.disposition + '\t' +
           (attachment.download_complete ? "1" : "0") + '\t' + attachment.fetch_error;
}

std::optional<MessageAttachment> ParseAttachment(std::string_view value) {
    MessageAttachment attachment;
    std::string token;
    std::vector<std::string> parts;
    for (char ch : value) {
        if (ch == '\t') {
            parts.push_back(token);
            token.clear();
            continue;
        }
        token.push_back(ch);
    }
    parts.push_back(token);
    if (parts.size() < 4) {
        return std::nullopt;
    }
    attachment.name = parts[0];
    attachment.content_type = parts[1];
    try {
        attachment.size = static_cast<std::size_t>(std::stoull(parts[2]));
    } catch (...) {
        attachment.size = 0;
    }
    attachment.omitted = parts[3] == "1";
    if (parts.size() >= 5) {
        attachment.payload_path = parts[4];
    }
    if (parts.size() >= 6) {
        attachment.content_id = parts[5];
    }
    if (parts.size() >= 7) {
        attachment.disposition = parts[6];
    }
    if (parts.size() >= 8) {
        attachment.download_complete = parts[7] != "0";
    }
    if (parts.size() >= 9) {
        attachment.fetch_error = parts[8];
    }
    return attachment;
}

bool WriteBinaryFile(const std::filesystem::path& path,
                     std::string_view contents,
                     std::string* error_message) {
    std::error_code create_error;
    std::filesystem::create_directories(path.parent_path(), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create attachment directory: " + create_error.message();
        }
        return false;
    }

    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        if (error_message) {
            *error_message = "Unable to write attachment payload: " + path.string();
        }
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(output);
}

std::optional<std::string> ReadBinaryFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

bool FilesystemMessageStore::SaveMessage(const MessageRecord& message, std::string* error_message) {
    if (message.mailbox_id.empty() || message.id.empty()) {
        if (error_message) {
            *error_message = "Message mailbox id and message id must not be empty.";
        }
        return false;
    }

    std::error_code create_error;
    std::filesystem::create_directories(MailboxDirectory(message.mailbox_id), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create mailbox directory: " + create_error.message();
        }
        return false;
    }

    std::vector<MessageAttachment> attachments = message.attachments;
    for (std::size_t index = 0; index < attachments.size(); ++index) {
        auto& attachment = attachments[index];
        const std::string suggested_name = attachment.name.empty() ? "attachment.bin" : attachment.name;
        const std::filesystem::path managed_path =
            AttachmentStoragePath(root_directory_, message.id, index, suggested_name);

        if (!attachment.payload_path.empty()) {
            const std::filesystem::path source_path = attachment.payload_path;
            std::error_code exists_error;
            if (std::filesystem::exists(source_path, exists_error) && !exists_error) {
                std::error_code create_attachment_error;
                std::filesystem::create_directories(managed_path.parent_path(), create_attachment_error);
                if (create_attachment_error) {
                    if (error_message) {
                        *error_message = "Unable to create attachment directory: " +
                                         create_attachment_error.message();
                    }
                    return false;
                }
                std::error_code copy_error;
                if (source_path != managed_path) {
                    std::filesystem::copy_file(source_path,
                                               managed_path,
                                               std::filesystem::copy_options::overwrite_existing,
                                               copy_error);
                    if (copy_error) {
                        if (error_message) {
                            *error_message = "Unable to import attachment payload: " + copy_error.message();
                        }
                        return false;
                    }
                }
                attachment.payload_path = managed_path.string();
            } else if (!attachment.omitted && attachment.download_complete) {
                if (error_message) {
                    *error_message = "Attachment payload is unavailable: " + source_path.string();
                }
                return false;
            }
        } else if (const auto existing = ExistingAttachmentPath(root_directory_, message.id, index)) {
            attachment.payload_path = existing->string();
        }
    }

    std::ofstream output(MessagePath(message.mailbox_id, message.id));
    if (!output.is_open()) {
        if (error_message) {
            *error_message = "Unable to write message file.";
        }
        return false;
    }

    output << "Subject: " << message.subject << '\n';
    output << "From: " << message.sender << '\n';
    output << "To: " << message.recipients << '\n';
    output << "X-Hermes-Message-Id: " << message.id << '\n';
    output << "X-Hermes-Account-Id: " << message.account_id << '\n';
    output << "X-Hermes-Delivery-State: " << DeliveryStateToString(message.delivery_state) << '\n';
    output << "X-Hermes-Remote-Id: " << message.remote_id << '\n';
    output << "X-Hermes-Remote-Mailbox: " << message.remote_mailbox << '\n';
    output << "X-Hermes-Download-Complete: " << (message.download_complete ? "1" : "0") << '\n';
    output << "X-Hermes-Attachments-Omitted: " << (message.attachments_omitted ? "1" : "0") << '\n';
    output << "X-Hermes-Flagged: " << (message.flagged ? "1" : "0") << '\n';
    output << "X-Hermes-Deleted: " << (message.deleted ? "1" : "0") << '\n';
    output << "X-Hermes-Answered: " << (message.answered ? "1" : "0") << '\n';
    output << "X-Hermes-Last-Error: " << message.last_error << '\n';
    output << "X-Hermes-Created-At: " << message.created_at << '\n';
    output << "X-Hermes-Updated-At: " << message.updated_at << '\n';
    output << "X-Hermes-Unread: " << (message.unread ? "1" : "0") << '\n';
    for (const auto& attachment : attachments) {
        output << "X-Hermes-Attachment: " << SerializeAttachment(attachment) << '\n';
    }
    output << '\n';
    output << message.plain_text_body;
    if (!message.html_body.empty()) {
        output << kHtmlMarker << message.html_body;
    }
    return true;
}

std::vector<MessageRecord> FilesystemMessageStore::ListMessages(std::string_view mailbox_id) const {
    std::vector<MessageRecord> messages;
    const std::filesystem::path directory = MailboxDirectory(mailbox_id);
    if (!std::filesystem::exists(directory)) {
        return messages;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".eml") {
            continue;
        }

        if (auto record = ReadMessageFile(entry.path(), mailbox_id)) {
            for (std::size_t index = 0; index < record->attachments.size(); ++index) {
                if (record->attachments[index].payload_path.empty()) {
                    if (const auto existing = ExistingAttachmentPath(root_directory_, record->id, index)) {
                        record->attachments[index].payload_path = existing->string();
                    }
                }
            }
            messages.push_back(std::move(*record));
        }
    }

    std::sort(messages.begin(),
              messages.end(),
              [](const MessageRecord& left, const MessageRecord& right) {
                  return left.id < right.id;
              });
    return messages;
}

std::optional<MessageRecord> FilesystemMessageStore::GetMessage(std::string_view mailbox_id,
                                                                std::string_view message_id) const {
    auto record = ReadMessageFile(MessagePath(mailbox_id, message_id), mailbox_id);
    if (!record) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < record->attachments.size(); ++index) {
        if (record->attachments[index].payload_path.empty()) {
            if (const auto existing = ExistingAttachmentPath(root_directory_, record->id, index)) {
                record->attachments[index].payload_path = existing->string();
            }
        }
    }
    return record;
}

bool FilesystemMessageStore::DeleteMessage(std::string_view mailbox_id,
                                           std::string_view message_id,
                                           std::string* error_message) {
    std::error_code remove_error;
    const bool removed = std::filesystem::remove(MessagePath(mailbox_id, message_id), remove_error);
    if (!removed && remove_error && error_message) {
        *error_message = "Unable to delete message: " + remove_error.message();
    }
    return removed || !remove_error;
}

bool FilesystemMessageStore::CopyMessage(std::string_view source_mailbox_id,
                                         std::string_view message_id,
                                         std::string_view destination_mailbox_id,
                                         std::string* error_message) {
    const auto source = GetMessage(source_mailbox_id, message_id);
    if (!source) {
        if (error_message) {
            *error_message = "Unable to find message to copy.";
        }
        return false;
    }
    MessageRecord copied = *source;
    copied.mailbox_id = std::string(destination_mailbox_id);
    return SaveMessage(copied, error_message);
}

bool FilesystemMessageStore::MoveMessage(std::string_view source_mailbox_id,
                                         std::string_view message_id,
                                         std::string_view destination_mailbox_id,
                                         std::string* error_message) {
    const auto source_path = MessagePath(source_mailbox_id, message_id);
    const auto destination_directory = MailboxDirectory(destination_mailbox_id);
    std::error_code create_error;
    std::filesystem::create_directories(destination_directory, create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create destination mailbox directory: " + create_error.message();
        }
        return false;
    }

    const auto destination_path = MessagePath(destination_mailbox_id, message_id);
    std::error_code rename_error;
    std::filesystem::rename(source_path, destination_path, rename_error);
    if (!rename_error) {
        return true;
    }

    if (!CopyMessage(source_mailbox_id, message_id, destination_mailbox_id, error_message)) {
        return false;
    }
    return DeleteMessage(source_mailbox_id, message_id, error_message);
}

bool FilesystemMessageStore::SaveAttachmentPayload(std::string_view mailbox_id,
                                                   std::string_view message_id,
                                                   std::size_t attachment_index,
                                                   std::string_view suggested_name,
                                                   std::string_view payload,
                                                   std::string* error_message) {
    (void)mailbox_id;
    return WriteBinaryFile(AttachmentStoragePath(root_directory_,
                                                 message_id,
                                                 attachment_index,
                                                 suggested_name.empty() ? "attachment.bin"
                                                                        : std::string(suggested_name)),
                           payload,
                           error_message);
}

bool FilesystemMessageStore::ImportAttachmentFile(std::string_view mailbox_id,
                                                  std::string_view message_id,
                                                  std::size_t attachment_index,
                                                  const std::filesystem::path& source_path,
                                                  std::string* error_message) {
    (void)mailbox_id;
    if (source_path.empty()) {
        if (error_message) {
            *error_message = "Attachment source path must not be empty.";
        }
        return false;
    }
    const auto destination =
        AttachmentStoragePath(root_directory_, message_id, attachment_index, source_path.filename().string());
    std::error_code create_error;
    std::filesystem::create_directories(destination.parent_path(), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create attachment directory: " + create_error.message();
        }
        return false;
    }
    std::error_code copy_error;
    std::filesystem::copy_file(source_path,
                               destination,
                               std::filesystem::copy_options::overwrite_existing,
                               copy_error);
    if (copy_error) {
        if (error_message) {
            *error_message = "Unable to import attachment payload: " + copy_error.message();
        }
        return false;
    }
    return true;
}

std::optional<std::string> FilesystemMessageStore::LoadAttachmentPayload(std::string_view mailbox_id,
                                                                         std::string_view message_id,
                                                                         std::size_t attachment_index) const {
    (void)mailbox_id;
    const auto path = AttachmentPath(mailbox_id, message_id, attachment_index);
    return path ? ReadBinaryFile(*path) : std::nullopt;
}

std::optional<std::filesystem::path> FilesystemMessageStore::AttachmentPath(std::string_view mailbox_id,
                                                                            std::string_view message_id,
                                                                            std::size_t attachment_index) const {
    (void)mailbox_id;
    return ExistingAttachmentPath(root_directory_, message_id, attachment_index);
}

std::filesystem::path FilesystemMessageStore::MailboxDirectory(std::string_view mailbox_id) const {
    return root_directory_ / "mailboxes" / std::string(mailbox_id);
}

std::filesystem::path FilesystemMessageStore::MessagePath(std::string_view mailbox_id,
                                                          std::string_view message_id) const {
    return MailboxDirectory(mailbox_id) / (std::string(message_id) + ".eml");
}

std::filesystem::path FilesystemMessageStore::AttachmentsDirectory(std::string_view message_id) const {
    return root_directory_ / "Attachments" / std::string(message_id);
}

std::optional<MessageRecord> FilesystemMessageStore::ReadMessageFile(const std::filesystem::path& path,
                                                                     std::string_view mailbox_id) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return std::nullopt;
    }

    MessageRecord message;
    message.mailbox_id = std::string(mailbox_id);
    message.id = path.stem().string();

    std::string line;
    bool in_headers = true;
    std::ostringstream body_stream;
    while (std::getline(input, line)) {
        if (in_headers) {
            if (line.empty() || line == "\r") {
                in_headers = false;
                continue;
            }

            if (line.rfind("Subject:", 0) == 0) {
                message.subject = HeaderValue(line);
            } else if (line.rfind("From:", 0) == 0) {
                message.sender = HeaderValue(line);
            } else if (line.rfind("To:", 0) == 0) {
                message.recipients = HeaderValue(line);
            } else if (line.rfind("X-Hermes-Message-Id:", 0) == 0) {
                message.id = HeaderValue(line);
            } else if (line.rfind("X-Hermes-Account-Id:", 0) == 0) {
                message.account_id = HeaderValue(line);
            } else if (line.rfind("X-Hermes-Delivery-State:", 0) == 0) {
                message.delivery_state = DeliveryStateFromString(HeaderValue(line));
            } else if (line.rfind("X-Hermes-Remote-Id:", 0) == 0) {
                message.remote_id = HeaderValue(line);
            } else if (line.rfind("X-Hermes-Remote-Mailbox:", 0) == 0) {
                message.remote_mailbox = HeaderValue(line);
            } else if (line.rfind("X-Hermes-Download-Complete:", 0) == 0) {
                message.download_complete = HeaderValue(line) != "0";
            } else if (line.rfind("X-Hermes-Attachments-Omitted:", 0) == 0) {
                message.attachments_omitted = HeaderValue(line) != "0";
            } else if (line.rfind("X-Hermes-Flagged:", 0) == 0) {
                message.flagged = HeaderValue(line) != "0";
            } else if (line.rfind("X-Hermes-Deleted:", 0) == 0) {
                message.deleted = HeaderValue(line) != "0";
            } else if (line.rfind("X-Hermes-Answered:", 0) == 0) {
                message.answered = HeaderValue(line) != "0";
            } else if (line.rfind("X-Hermes-Last-Error:", 0) == 0) {
                message.last_error = HeaderValue(line);
            } else if (line.rfind("X-Hermes-Created-At:", 0) == 0) {
                try {
                    message.created_at = std::stoll(HeaderValue(line));
                } catch (...) {
                    message.created_at = 0;
                }
            } else if (line.rfind("X-Hermes-Updated-At:", 0) == 0) {
                try {
                    message.updated_at = std::stoll(HeaderValue(line));
                } catch (...) {
                    message.updated_at = 0;
                }
            } else if (line.rfind("X-Hermes-Unread:", 0) == 0) {
                message.unread = HeaderValue(line) != "0";
            } else if (line.rfind("X-Hermes-Attachment:", 0) == 0) {
                if (const auto attachment = ParseAttachment(HeaderValue(line))) {
                    message.attachments.push_back(*attachment);
                }
            }
            continue;
        }

        body_stream << line;
        if (!input.eof()) {
            body_stream << '\n';
        }
    }

    std::string body = TrimRight(body_stream.str());
    const std::size_t marker = body.find(kHtmlMarker);
    if (marker == std::string::npos) {
        message.plain_text_body = body;
    } else {
        message.plain_text_body = body.substr(0, marker);
        message.html_body = body.substr(marker + std::char_traits<char>::length(kHtmlMarker));
    }

    return message;
}

}  // namespace hermes
