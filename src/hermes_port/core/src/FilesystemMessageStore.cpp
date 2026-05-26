#include "hermes/MessageStore.h"

#include <algorithm>
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
    output << "X-Hermes-Unread: " << (message.unread ? "1" : "0") << '\n';
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
            } else if (line.rfind("X-Hermes-Unread:", 0) == 0) {
                message.unread = HeaderValue(line) != "0";
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
