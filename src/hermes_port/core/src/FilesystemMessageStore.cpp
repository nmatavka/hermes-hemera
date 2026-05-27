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
    while (start < line.size() &&
           (line[start] == ' ' || line[start] == '\t')) {
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

std::string SerializeAttachment(const MessageAttachment& attachment) {
    return attachment.name + '\t' + attachment.content_type + '\t' + std::to_string(attachment.size) + '\t' +
           (attachment.omitted ? "1" : "0");
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
    return attachment;
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
    for (const auto& attachment : message.attachments) {
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
    return ReadMessageFile(MessagePath(mailbox_id, message_id), mailbox_id);
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

    if (auto message = ReadMessageFile(source_path, source_mailbox_id)) {
        message->mailbox_id = std::string(destination_mailbox_id);
        if (!SaveMessage(*message, error_message)) {
            return false;
        }
        return DeleteMessage(source_mailbox_id, message_id, error_message);
    }

    if (error_message) {
        *error_message = "Unable to move message: " + rename_error.message();
    }
    return false;
}

std::filesystem::path FilesystemMessageStore::MailboxDirectory(std::string_view mailbox_id) const {
    return root_directory_ / "mailboxes" / std::string(mailbox_id);
}

std::filesystem::path FilesystemMessageStore::MessagePath(std::string_view mailbox_id,
                                                          std::string_view message_id) const {
    return MailboxDirectory(mailbox_id) / (std::string(message_id) + ".eml");
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
