#include "hermes/MailTransportCoordinator.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if HERMES_HAS_OPENSSL
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#endif

namespace hermes {

namespace {

struct ParsedHeaders {
    std::string subject;
    std::string from;
    std::string to;
    std::string content_type;
};

struct ParsedMimeMessage {
    ParsedHeaders headers;
    std::string plain_text_body;
    std::string html_body;
    std::vector<MessageAttachment> attachments;
};

std::int64_t NowUnixSeconds() {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::string GenerateId(std::string_view prefix) {
    return std::string(prefix) + "-" + std::to_string(static_cast<long long>(NowUnixSeconds())) + "-" +
           std::to_string(static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
}

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool StartsWithInsensitive(std::string_view value, std::string_view prefix) {
    if (prefix.size() > value.size()) {
        return false;
    }
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(value[index])) !=
            std::tolower(static_cast<unsigned char>(prefix[index]))) {
            return false;
        }
    }
    return true;
}

std::string TrimQuotes(std::string value) {
    value = Trim(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

TransportSecurity MapSecurity(TransportSecurityMode mode) {
    switch (mode) {
        case TransportSecurityMode::kPlaintext:
            return TransportSecurity::kPlaintext;
        case TransportSecurityMode::kImplicitTls:
            return TransportSecurity::kImplicitTls;
        case TransportSecurityMode::kStartTls:
            return TransportSecurity::kStartTls;
    }
    return TransportSecurity::kPlaintext;
}

std::string Base64Encode(const std::string& input) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((input.size() + 2) / 3) * 4);

    int val = 0;
    int valb = -6;
    for (unsigned char ch : input) {
        val = (val << 8) + ch;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(kAlphabet[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        encoded.push_back(kAlphabet[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (encoded.size() % 4 != 0) {
        encoded.push_back('=');
    }
    return encoded;
}

std::string Base64Decode(const std::string& input) {
    static const std::string kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<int> lookup(256, -1);
    for (std::size_t index = 0; index < kAlphabet.size(); ++index) {
        lookup[static_cast<unsigned char>(kAlphabet[index])] = static_cast<int>(index);
    }

    std::string decoded;
    int val = 0;
    int valb = -8;
    for (unsigned char ch : input) {
        if (lookup[ch] == -1) {
            if (ch == '=') {
                break;
            }
            continue;
        }
        val = (val << 6) + lookup[ch];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded;
}

std::string HexEncode(const unsigned char* data, std::size_t size) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string output;
    output.reserve(size * 2);
    for (std::size_t index = 0; index < size; ++index) {
        output.push_back(kHex[(data[index] >> 4) & 0x0F]);
        output.push_back(kHex[data[index] & 0x0F]);
    }
    return output;
}

std::string Md5Hex(const std::string& input) {
#if HERMES_HAS_OPENSSL
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
    return HexEncode(digest, sizeof(digest));
#else
    (void)input;
    return {};
#endif
}

std::string HmacMd5Hex(const std::string& key, const std::string& input) {
#if HERMES_HAS_OPENSSL
    unsigned int digest_length = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];
    HMAC(EVP_md5(),
         key.data(),
         static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(input.data()),
         input.size(),
         digest,
         &digest_length);
    return HexEncode(digest, digest_length);
#else
    (void)key;
    (void)input;
    return {};
#endif
}

std::vector<std::string> SplitRecipients(std::string_view value) {
    std::vector<std::string> recipients;
    std::string current;
    for (char ch : value) {
        if (ch == ',' || ch == ';' || ch == '\n' || ch == '\r') {
            const std::string token = Trim(current);
            if (!token.empty()) {
                recipients.push_back(token);
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    const std::string token = Trim(current);
    if (!token.empty()) {
        recipients.push_back(token);
    }
    return recipients;
}

std::string ExtractEmail(std::string value) {
    value = Trim(std::move(value));
    const auto open = value.find('<');
    const auto close = value.find('>');
    if (open != std::string::npos && close != std::string::npos && close > open) {
        return Trim(value.substr(open + 1, close - open - 1));
    }
    return value;
}

std::string SanitizeId(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(ch);
        } else {
            result.push_back('-');
        }
    }
    return result;
}

std::string PopMessageId(std::string_view account_id, std::string_view uidl) {
    return "pop-" + SanitizeId(account_id) + "-" + SanitizeId(uidl);
}

std::string ImapMessageId(std::string_view account_id, std::string_view mailbox_id, std::uint64_t uid) {
    return "imap-" + SanitizeId(account_id) + "-" + SanitizeId(mailbox_id) + "-" + std::to_string(uid);
}

std::string LocalMailboxIdForImap(std::string_view account_id, std::string_view remote_name) {
    return std::string(account_id) + ":" + SanitizeId(remote_name);
}

std::string ParameterValue(std::string_view header_value, std::string_view parameter_name) {
    const std::string lowered = ToLower(std::string(header_value));
    const std::string key = ToLower(std::string(parameter_name)) + "=";
    const std::size_t position = lowered.find(key);
    if (position == std::string::npos) {
        return {};
    }
    std::size_t value_start = position + key.size();
    std::size_t value_end = header_value.find(';', value_start);
    if (value_end == std::string::npos) {
        value_end = header_value.size();
    }
    return TrimQuotes(std::string(header_value.substr(value_start, value_end - value_start)));
}

ParsedHeaders ParseTopHeaders(const std::string& raw_message, std::size_t* body_offset) {
    ParsedHeaders headers;
    std::istringstream stream(raw_message);
    std::string line;
    std::size_t offset = 0;
    while (std::getline(stream, line)) {
        offset += line.size() + 1;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }
        if (StartsWithInsensitive(line, "Subject:")) {
            headers.subject = Trim(line.substr(8));
        } else if (StartsWithInsensitive(line, "From:")) {
            headers.from = Trim(line.substr(5));
        } else if (StartsWithInsensitive(line, "To:")) {
            headers.to = Trim(line.substr(3));
        } else if (StartsWithInsensitive(line, "Content-Type:")) {
            headers.content_type = Trim(line.substr(13));
        }
    }
    if (body_offset) {
        *body_offset = offset;
    }
    return headers;
}

std::pair<std::map<std::string, std::string>, std::string> ParsePart(const std::string& part) {
    std::map<std::string, std::string> headers;
    std::istringstream stream(part);
    std::string line;
    bool in_headers = true;
    std::string body;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (in_headers) {
            if (line.empty()) {
                in_headers = false;
                continue;
            }
            const std::size_t colon = line.find(':');
            if (colon != std::string::npos) {
                headers[ToLower(Trim(line.substr(0, colon)))] = Trim(line.substr(colon + 1));
            }
            continue;
        }
        body += line;
        if (!stream.eof()) {
            body += '\n';
        }
    }
    return {headers, body};
}

ParsedMimeMessage ParseMimeMessage(const std::string& raw_message) {
    ParsedMimeMessage parsed;
    std::size_t body_offset = 0;
    parsed.headers = ParseTopHeaders(raw_message, &body_offset);
    const std::string body = raw_message.substr(std::min(body_offset, raw_message.size()));
    const std::string content_type = ToLower(parsed.headers.content_type);

    if (content_type.find("multipart/") == std::string::npos) {
        if (content_type.find("text/html") != std::string::npos) {
            parsed.html_body = body;
        } else {
            parsed.plain_text_body = body;
        }
        return parsed;
    }

    const std::string boundary = ParameterValue(parsed.headers.content_type, "boundary");
    if (boundary.empty()) {
        parsed.plain_text_body = body;
        return parsed;
    }

    const std::string delimiter = "--" + boundary;
    std::size_t offset = 0;
    while (true) {
        const std::size_t begin = body.find(delimiter, offset);
        if (begin == std::string::npos) {
            break;
        }
        std::size_t content_begin = begin + delimiter.size();
        if (content_begin + 1 < body.size() && body[content_begin] == '-' && body[content_begin + 1] == '-') {
            break;
        }
        if (content_begin < body.size() && body[content_begin] == '\r') {
            ++content_begin;
        }
        if (content_begin < body.size() && body[content_begin] == '\n') {
            ++content_begin;
        }
        const std::size_t next = body.find(delimiter, content_begin);
        const std::string part = body.substr(content_begin, next == std::string::npos ? std::string::npos : next - content_begin);
        offset = next == std::string::npos ? body.size() : next;

        const auto [headers, part_body] = ParsePart(part);
        const std::string part_type = ToLower(headers.count("content-type") ? headers.at("content-type") : "text/plain");
        const std::string disposition =
            ToLower(headers.count("content-disposition") ? headers.at("content-disposition") : "");

        const bool is_attachment = disposition.find("attachment") != std::string::npos ||
                                   !ParameterValue(headers.count("content-disposition") ? headers.at("content-disposition") : "",
                                                   "filename")
                                        .empty() ||
                                   !ParameterValue(headers.count("content-type") ? headers.at("content-type") : "", "name")
                                        .empty();

        if (is_attachment) {
            MessageAttachment attachment;
            attachment.name =
                ParameterValue(headers.count("content-disposition") ? headers.at("content-disposition") : "", "filename");
            if (attachment.name.empty()) {
                attachment.name =
                    ParameterValue(headers.count("content-type") ? headers.at("content-type") : "", "name");
            }
            if (attachment.name.empty()) {
                attachment.name = "attachment";
            }
            attachment.content_type = headers.count("content-type") ? headers.at("content-type") : "application/octet-stream";
            attachment.size = part_body.size();
            parsed.attachments.push_back(std::move(attachment));
            continue;
        }

        if (part_type.find("text/html") != std::string::npos) {
            parsed.html_body = part_body;
        } else if (part_type.find("text/plain") != std::string::npos) {
            parsed.plain_text_body = part_body;
        }
    }

    if (parsed.plain_text_body.empty() && !parsed.html_body.empty()) {
        parsed.plain_text_body = parsed.html_body;
    }
    return parsed;
}

MessageRecord BuildReceivedMessage(const std::string& raw_message,
                                   const AccountProfile& account,
                                   std::string_view mailbox_id,
                                   std::string_view remote_id,
                                   std::string_view remote_mailbox,
                                   bool unread,
                                   bool download_complete,
                                   bool flagged = false,
                                   bool deleted = false,
                                   bool answered = false) {
    const ParsedMimeMessage parsed = ParseMimeMessage(raw_message);
    MessageRecord record;
    record.id = remote_mailbox == "INBOX" || remote_mailbox.empty()
                    ? PopMessageId(account.id, remote_id)
                    : ImapMessageId(account.id, mailbox_id, static_cast<std::uint64_t>(std::stoull(std::string(remote_id))));
    record.mailbox_id = std::string(mailbox_id);
    record.account_id = account.id;
    record.subject = parsed.headers.subject;
    record.sender = parsed.headers.from;
    record.recipients = parsed.headers.to;
    record.plain_text_body = parsed.plain_text_body;
    record.html_body = parsed.html_body;
    record.delivery_state = MessageDeliveryState::kReceived;
    record.remote_id = std::string(remote_id);
    record.remote_mailbox = std::string(remote_mailbox);
    record.download_complete = download_complete;
    record.attachments = parsed.attachments;
    record.attachments_omitted = account.imap_omit_attachments && !record.attachments.empty();
    record.flagged = flagged;
    record.deleted = deleted;
    record.answered = answered;
    record.unread = unread;
    record.created_at = NowUnixSeconds();
    record.updated_at = record.created_at;

    if (account.imap_max_download_size > 0 && record.plain_text_body.size() > account.imap_max_download_size) {
        record.plain_text_body.resize(account.imap_max_download_size);
        record.download_complete = false;
    }
    return record;
}

std::string BuildSmtpMessage(const MessageRecord& message) {
    std::ostringstream output;
    output << "Subject: " << message.subject << "\r\n";
    output << "From: " << message.sender << "\r\n";
    output << "To: " << message.recipients << "\r\n";
    output << "MIME-Version: 1.0\r\n";
    if (!message.html_body.empty()) {
        output << "Content-Type: multipart/alternative; boundary=\"hermes-boundary\"\r\n\r\n";
        output << "--hermes-boundary\r\n";
        output << "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
        output << message.plain_text_body << "\r\n";
        output << "--hermes-boundary\r\n";
        output << "Content-Type: text/html; charset=UTF-8\r\n\r\n";
        output << message.html_body << "\r\n";
        output << "--hermes-boundary--\r\n";
    } else {
        output << "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
        output << message.plain_text_body << "\r\n";
    }
    return output.str();
}

std::string DotStuff(std::string message) {
    std::string result;
    result.reserve(message.size() + 32);
    bool at_line_start = true;
    for (char ch : message) {
        if (at_line_start && ch == '.') {
            result.push_back('.');
        }
        result.push_back(ch);
        at_line_start = ch == '\n';
    }
    return result;
}

std::string DisplayName(const AccountProfile& account) {
    return account.display_name.empty() ? account.id : account.display_name;
}

bool ReadDotTerminatedBlock(TransportConnection& connection,
                            std::string* block,
                            std::string* error_message) {
    if (!block) {
        return false;
    }
    block->clear();
    std::string line;
    while (connection.ReceiveLine(&line, error_message)) {
        if (line == ".") {
            return true;
        }
        if (StartsWithInsensitive(line, "..")) {
            line.erase(line.begin());
        }
        *block += line;
        *block += "\n";
    }
    return false;
}

class TaskScope {
public:
    TaskScope(MailTaskModel& task_model,
              MailTaskKind kind,
              std::string persona,
              std::string title,
              std::string status)
        : task_model_(task_model) {
        task_.id = GenerateId("task");
        task_.kind = kind;
        task_.persona = std::move(persona);
        task_.title = std::move(title);
        task_.status = std::move(status);
        task_.state = MailTaskState::kRunning;
        task_model_.UpsertTask(task_);
    }

    void SetProgress(int so_far, int total, std::string status) {
        task_.so_far = so_far;
        task_.total = total;
        task_.status = std::move(status);
        task_model_.UpsertTask(task_);
    }

    void Complete(std::string status) {
        task_model_.CompleteTask(task_.id, status);
        completed_ = true;
    }

    void Fail(std::string status, std::string error_message) {
        task_model_.FailTask(task_.id, status, error_message);
        completed_ = true;
    }

    ~TaskScope() {
        if (!completed_) {
            task_model_.CompleteTask(task_.id, task_.status);
        }
    }

private:
    MailTaskModel& task_model_;
    MailTaskRecord task_;
    bool completed_ = false;
};

class SmtpSession {
public:
    SmtpSession(const AccountProfile& account,
                std::string password,
                TransportService& transport_service,
                const TlsProvider& tls_provider)
        : account_(account),
          password_(std::move(password)),
          transport_service_(transport_service),
          tls_provider_(tls_provider) {}

    bool SendMessage(const MessageRecord& message, std::string* error_message) {
        auto connection = transport_service_.Connect(
            {account_.outgoing_server, account_.outgoing_port, MapSecurity(account_.outgoing_security), 5000},
            error_message);
        if (!connection) {
            return false;
        }

        std::string line;
        if (!connection->ReceiveLine(&line, error_message) || line.rfind("220", 0) != 0) {
            if (error_message && error_message->empty()) {
                *error_message = "SMTP server did not send a 220 greeting.";
            }
            return false;
        }

        std::vector<std::string> ehlo_lines;
        if (!Ehlo(*connection, &ehlo_lines, error_message)) {
            return false;
        }

        if (account_.outgoing_security == TransportSecurityMode::kStartTls && !connection->IsSecure()) {
            if (!SendExpect(*connection, "STARTTLS\r\n", "220", error_message)) {
                return false;
            }
            if (!connection->UpgradeToTls(tls_provider_, account_.outgoing_server, error_message)) {
                return false;
            }
            ehlo_lines.clear();
            if (!Ehlo(*connection, &ehlo_lines, error_message)) {
                return false;
            }
        }

        if (!Authenticate(*connection, ehlo_lines, error_message)) {
            return false;
        }

        const std::string sender = ExtractEmail(message.sender.empty() ? account_.email_address : message.sender);
        if (!SendExpect(*connection, "MAIL FROM:<" + sender + ">\r\n", "250", error_message)) {
            return false;
        }

        for (const auto& recipient_token : SplitRecipients(message.recipients)) {
            const std::string recipient = ExtractEmail(recipient_token);
            if (!SendExpect(*connection, "RCPT TO:<" + recipient + ">\r\n", "250", error_message)) {
                return false;
            }
        }

        if (!SendExpect(*connection, "DATA\r\n", "354", error_message)) {
            return false;
        }

        const std::string payload = DotStuff(BuildSmtpMessage(message));
        if (!connection->Send(payload + "\r\n.\r\n", error_message)) {
            return false;
        }
        if (!connection->ReceiveLine(&line, error_message) || line.rfind("250", 0) != 0) {
            if (error_message && error_message->empty()) {
                *error_message = "SMTP server rejected the message body.";
            }
            return false;
        }

        SendExpect(*connection, "QUIT\r\n", "221", nullptr);
        return true;
    }

private:
    bool Ehlo(TransportConnection& connection, std::vector<std::string>* lines, std::string* error_message) {
        if (!connection.Send("EHLO hermes-hemera\r\n", error_message)) {
            return false;
        }

        std::string line;
        while (connection.ReceiveLine(&line, error_message)) {
            if (line.rfind("250-", 0) == 0) {
                if (lines) {
                    lines->push_back(line.substr(4));
                }
                continue;
            }
            if (line.rfind("250 ", 0) == 0) {
                if (lines) {
                    lines->push_back(line.substr(4));
                }
                return true;
            }
            if (error_message) {
                *error_message = "SMTP EHLO failed: " + line;
            }
            return false;
        }
        return false;
    }

    bool Authenticate(TransportConnection& connection,
                      const std::vector<std::string>& ehlo_lines,
                      std::string* error_message) {
        const std::string auth_capabilities = [&]() {
            for (const auto& line : ehlo_lines) {
                if (StartsWithInsensitive(line, "AUTH ")) {
                    return ToLower(line.substr(5));
                }
            }
            return std::string();
        }();

        if (account_.smtp_auth == SmtpAuthMode::kNone || auth_capabilities.empty()) {
            return true;
        }

        if (account_.smtp_auth == SmtpAuthMode::kCramMd5 &&
            auth_capabilities.find("cram-md5") != std::string::npos) {
            if (!connection.Send("AUTH CRAM-MD5\r\n", error_message)) {
                return false;
            }
            std::string line;
            if (!connection.ReceiveLine(&line, error_message) || line.rfind("334 ", 0) != 0) {
                if (error_message) {
                    *error_message = "SMTP CRAM-MD5 challenge was not received.";
                }
                return false;
            }
            const std::string challenge = Base64Decode(line.substr(4));
            const std::string response =
                Base64Encode(account_.login_name + " " + HmacMd5Hex(password_, challenge));
            if (!connection.Send(response + "\r\n", error_message)) {
                return false;
            }
            if (!connection.ReceiveLine(&line, error_message) || line.rfind("235", 0) != 0) {
                if (error_message) {
                    *error_message = "SMTP CRAM-MD5 authentication failed.";
                }
                return false;
            }
            return true;
        }

        if (account_.smtp_auth == SmtpAuthMode::kLogin && auth_capabilities.find("login") != std::string::npos) {
            if (!connection.Send("AUTH LOGIN\r\n", error_message)) {
                return false;
            }
            std::string line;
            if (!connection.ReceiveLine(&line, error_message) || line.rfind("334", 0) != 0) {
                return false;
            }
            if (!connection.Send(Base64Encode(account_.login_name) + "\r\n", error_message)) {
                return false;
            }
            if (!connection.ReceiveLine(&line, error_message) || line.rfind("334", 0) != 0) {
                return false;
            }
            if (!connection.Send(Base64Encode(password_) + "\r\n", error_message)) {
                return false;
            }
            if (!connection.ReceiveLine(&line, error_message) || line.rfind("235", 0) != 0) {
                if (error_message) {
                    *error_message = "SMTP LOGIN authentication failed.";
                }
                return false;
            }
            return true;
        }

        if (auth_capabilities.find("plain") != std::string::npos) {
            const std::string payload = Base64Encode("\0" + account_.login_name + "\0" + password_);
            return SendExpect(connection, "AUTH PLAIN " + payload + "\r\n", "235", error_message);
        }

        if (error_message) {
            *error_message = "SMTP server does not support the configured authentication mode.";
        }
        return false;
    }

    bool SendExpect(TransportConnection& connection,
                    const std::string& command,
                    std::string_view expected_prefix,
                    std::string* error_message) {
        if (!connection.Send(command, error_message)) {
            return false;
        }
        std::string line;
        if (!connection.ReceiveLine(&line, error_message)) {
            return false;
        }
        if (line.rfind(std::string(expected_prefix), 0) != 0) {
            if (error_message) {
                *error_message = "SMTP unexpected response: " + line;
            }
            return false;
        }
        return true;
    }

    const AccountProfile& account_;
    std::string password_;
    TransportService& transport_service_;
    const TlsProvider& tls_provider_;
};

class PopSession {
public:
    PopSession(const AccountProfile& account,
               std::string password,
               TransportService& transport_service,
               const TlsProvider& tls_provider)
        : account_(account),
          password_(std::move(password)),
          transport_service_(transport_service),
          tls_provider_(tls_provider) {}

    bool FetchMessages(PopSyncState* state,
                       std::vector<MessageRecord>* messages,
                       std::string* error_message) {
        auto connection = transport_service_.Connect(
            {account_.incoming_server, account_.incoming_port, MapSecurity(account_.incoming_security), 5000},
            error_message);
        if (!connection) {
            return false;
        }

        std::string greeting;
        if (!connection->ReceiveLine(&greeting, error_message) || greeting.rfind("+OK", 0) != 0) {
            if (error_message) {
                *error_message = "POP server did not send a +OK greeting.";
            }
            return false;
        }

        if (account_.incoming_security == TransportSecurityMode::kStartTls) {
            if (!SendSimple(*connection, "STLS\r\n", "+OK", error_message)) {
                return false;
            }
            if (!connection->UpgradeToTls(tls_provider_, account_.incoming_server, error_message)) {
                return false;
            }
        }

        if (!Authenticate(*connection, greeting, error_message)) {
            return false;
        }

        std::map<int, std::string> uidls;
        if (!connection->Send("UIDL\r\n", error_message)) {
            return false;
        }
        std::string line;
        if (!connection->ReceiveLine(&line, error_message) || line.rfind("+OK", 0) != 0) {
            return false;
        }
        while (connection->ReceiveLine(&line, error_message) && line != ".") {
            std::istringstream line_stream(line);
            int index = 0;
            std::string uidl;
            line_stream >> index >> uidl;
            if (index > 0 && !uidl.empty()) {
                uidls[index] = uidl;
            }
        }

        std::map<int, std::size_t> sizes;
        if (!connection->Send("LIST\r\n", error_message)) {
            return false;
        }
        if (!connection->ReceiveLine(&line, error_message) || line.rfind("+OK", 0) != 0) {
            return false;
        }
        while (connection->ReceiveLine(&line, error_message) && line != ".") {
            std::istringstream line_stream(line);
            int index = 0;
            std::size_t size = 0;
            line_stream >> index >> size;
            if (index > 0) {
                sizes[index] = size;
            }
        }

        for (const auto& entry : uidls) {
            if (state && state->uidl_to_message_id.count(entry.second) > 0) {
                continue;
            }
            if (account_.skip_big_messages && account_.big_message_threshold > 0) {
                const auto size_it = sizes.find(entry.first);
                if (size_it != sizes.end() && size_it->second > account_.big_message_threshold) {
                    continue;
                }
            }

            if (!connection->Send("RETR " + std::to_string(entry.first) + "\r\n", error_message)) {
                return false;
            }
            if (!connection->ReceiveLine(&line, error_message) || line.rfind("+OK", 0) != 0) {
                return false;
            }

            std::string raw_message;
            if (!ReadDotTerminatedBlock(*connection, &raw_message, error_message)) {
                return false;
            }

            MessageRecord record =
                BuildReceivedMessage(raw_message, account_, "inbox", entry.second, "INBOX", true, true);
            record.id = PopMessageId(account_.id, entry.second);
            if (messages) {
                messages->push_back(record);
            }
            if (state) {
                state->uidl_to_message_id[entry.second] = record.id;
            }

            if (!account_.leave_mail_on_server || account_.delete_mail_from_server) {
                if (!SendSimple(*connection, "DELE " + std::to_string(entry.first) + "\r\n", "+OK", error_message)) {
                    return false;
                }
            }
        }

        SendSimple(*connection, "QUIT\r\n", "+OK", nullptr);
        return true;
    }

private:
    bool Authenticate(TransportConnection& connection,
                      const std::string& greeting,
                      std::string* error_message) {
        switch (account_.pop_auth) {
            case PopAuthMode::kPassword:
                return SendSimple(connection, "USER " + account_.login_name + "\r\n", "+OK", error_message) &&
                       SendSimple(connection, "PASS " + password_ + "\r\n", "+OK", error_message);

            case PopAuthMode::kAPOP: {
                const std::size_t start = greeting.find('<');
                const std::size_t end = greeting.find('>', start);
                if (start == std::string::npos || end == std::string::npos) {
                    if (error_message) {
                        *error_message = "POP APOP challenge is missing from the greeting.";
                    }
                    return false;
                }
                const std::string challenge = greeting.substr(start, end - start + 1);
                return SendSimple(connection,
                                  "APOP " + account_.login_name + " " + Md5Hex(challenge + password_) + "\r\n",
                                  "+OK",
                                  error_message);
            }

            case PopAuthMode::kKerberos:
                if (error_message) {
                    *error_message = "POP Kerberos authentication is not yet implemented.";
                }
                return false;

            case PopAuthMode::kRPA:
                if (error_message) {
                    *error_message = "POP RPA authentication is not yet implemented.";
                }
                return false;
        }
        return false;
    }

    bool SendSimple(TransportConnection& connection,
                    const std::string& command,
                    std::string_view expected_prefix,
                    std::string* error_message) {
        if (!connection.Send(command, error_message)) {
            return false;
        }
        std::string line;
        if (!connection.ReceiveLine(&line, error_message)) {
            return false;
        }
        if (line.rfind(std::string(expected_prefix), 0) != 0) {
            if (error_message) {
                *error_message = "POP unexpected response: " + line;
            }
            return false;
        }
        return true;
    }

    const AccountProfile& account_;
    std::string password_;
    TransportService& transport_service_;
    const TlsProvider& tls_provider_;
};

struct ImapFetchItem {
    std::uint64_t uid = 0;
    std::string flags;
    std::string raw_message;
};

class ImapSession {
public:
    ImapSession(const AccountProfile& account,
                std::string password,
                TransportService& transport_service,
                const TlsProvider& tls_provider)
        : account_(account),
          password_(std::move(password)),
          transport_service_(transport_service),
          tls_provider_(tls_provider) {}

    bool DiscoverMailboxes(std::vector<std::string>* remote_mailboxes, std::string* error_message) {
        if (!ConnectAndAuthenticate(error_message)) {
            return false;
        }

        std::vector<std::string> lines;
        if (!RunSimpleTagged("LIST \"\" \"*\"", &lines, error_message)) {
            return false;
        }
        for (const auto& line : lines) {
            if (!StartsWithInsensitive(line, "* LIST ")) {
                continue;
            }
            const std::size_t last_quote = line.rfind('"');
            if (last_quote == std::string::npos) {
                continue;
            }
            const std::size_t prev_quote = line.rfind('"', last_quote - 1);
            if (prev_quote == std::string::npos || prev_quote >= last_quote) {
                continue;
            }
            const std::string mailbox = line.substr(prev_quote + 1, last_quote - prev_quote - 1);
            if (!account_.imap_directory_prefix.empty() &&
                mailbox.rfind(account_.imap_directory_prefix, 0) != 0) {
                continue;
            }
            remote_mailboxes->push_back(mailbox);
        }
        if (std::find(remote_mailboxes->begin(), remote_mailboxes->end(), "INBOX") == remote_mailboxes->end()) {
            remote_mailboxes->insert(remote_mailboxes->begin(), "INBOX");
        }
        return true;
    }

    bool SyncMailbox(std::string_view remote_mailbox,
                     ImapMailboxSyncState* state,
                     std::vector<MessageRecord>* messages,
                     std::string* error_message) {
        if (!ConnectAndAuthenticate(error_message)) {
            return false;
        }

        std::vector<std::string> select_lines;
        if (!RunSimpleTagged("SELECT \"" + std::string(remote_mailbox) + "\"", &select_lines, error_message)) {
            return false;
        }

        std::uint64_t uid_validity = 0;
        for (const auto& line : select_lines) {
            const auto marker = line.find("[UIDVALIDITY ");
            if (marker != std::string::npos) {
                const std::size_t start = marker + 13;
                const std::size_t end = line.find(']', start);
                if (end != std::string::npos) {
                    uid_validity = static_cast<std::uint64_t>(std::stoull(line.substr(start, end - start)));
                }
            }
        }
        if (state && state->uid_validity != 0 && state->uid_validity != uid_validity) {
            state->last_seen_uid = 0;
        }
        if (state) {
            state->uid_validity = uid_validity;
        }

        const std::uint64_t next_uid = state ? state->last_seen_uid + 1 : 1;
        std::vector<ImapFetchItem> items;
        if (!FetchItems(next_uid, &items, error_message)) {
            return false;
        }

        std::uint64_t last_seen = state ? state->last_seen_uid : 0;
        for (const auto& item : items) {
            const std::string local_mailbox_id = LocalMailboxIdForImap(account_.id, remote_mailbox);
            MessageRecord record = BuildReceivedMessage(item.raw_message,
                                                       account_,
                                                       local_mailbox_id,
                                                       std::to_string(item.uid),
                                                       remote_mailbox,
                                                       item.flags.find("\\Seen") == std::string::npos,
                                                       true,
                                                       item.flags.find("\\Flagged") != std::string::npos,
                                                       item.flags.find("\\Deleted") != std::string::npos,
                                                       item.flags.find("\\Answered") != std::string::npos);
            record.id = ImapMessageId(account_.id, local_mailbox_id, item.uid);
            if (messages) {
                messages->push_back(record);
            }
            last_seen = std::max(last_seen, item.uid);
        }
        if (state) {
            state->last_seen_uid = last_seen;
        }
        return true;
    }

private:
    bool ConnectAndAuthenticate(std::string* error_message) {
        if (connection_) {
            return true;
        }
        connection_ = transport_service_.Connect(
            {account_.incoming_server, account_.incoming_port, MapSecurity(account_.incoming_security), 5000},
            error_message);
        if (!connection_) {
            return false;
        }

        std::string greeting;
        if (!connection_->ReceiveLine(&greeting, error_message) || !StartsWithInsensitive(greeting, "* OK")) {
            if (error_message) {
                *error_message = "IMAP server did not send an * OK greeting.";
            }
            return false;
        }

        if (account_.incoming_security == TransportSecurityMode::kStartTls) {
            std::vector<std::string> ignored;
            if (!RunSimpleTagged("STARTTLS", &ignored, error_message)) {
                return false;
            }
            if (!connection_->UpgradeToTls(tls_provider_, account_.incoming_server, error_message)) {
                return false;
            }
        }

        switch (account_.imap_auth) {
            case ImapAuthMode::kPassword:
                return RunSimpleTagged("LOGIN \"" + account_.login_name + "\" \"" + password_ + "\"",
                                       nullptr,
                                       error_message);

            case ImapAuthMode::kCramMd5: {
                const std::string tag = NextTag();
                if (!connection_->Send(tag + " AUTHENTICATE CRAM-MD5\r\n", error_message)) {
                    return false;
                }
                std::string line;
                if (!connection_->ReceiveLine(&line, error_message) || line.rfind("+ ", 0) != 0) {
                    if (error_message) {
                        *error_message = "IMAP CRAM-MD5 challenge was not received.";
                    }
                    return false;
                }
                const std::string challenge = Base64Decode(line.substr(2));
                const std::string response =
                    Base64Encode(account_.login_name + " " + HmacMd5Hex(password_, challenge));
                if (!connection_->Send(response + "\r\n", error_message)) {
                    return false;
                }
                return ReadTaggedResult(tag, nullptr, error_message);
            }

            case ImapAuthMode::kKerberos:
                if (error_message) {
                    *error_message = "IMAP Kerberos authentication is not yet implemented.";
                }
                return false;
        }
        return false;
    }

    bool FetchItems(std::uint64_t next_uid, std::vector<ImapFetchItem>* items, std::string* error_message) {
        const std::string tag = NextTag();
        if (!connection_->Send(tag + " UID FETCH " + std::to_string(next_uid) +
                                   ":* (UID FLAGS BODY.PEEK[])\r\n",
                               error_message)) {
            return false;
        }

        std::string line;
        while (connection_->ReceiveLine(&line, error_message)) {
            if (StartsWithInsensitive(line, tag + " OK")) {
                return true;
            }
            if (!StartsWithInsensitive(line, "* ") || line.find(" FETCH ") == std::string::npos) {
                continue;
            }

            ImapFetchItem item;
            const std::size_t uid_marker = line.find("UID ");
            if (uid_marker != std::string::npos) {
                std::size_t uid_end = line.find(' ', uid_marker + 4);
                if (uid_end == std::string::npos) {
                    uid_end = line.find(')', uid_marker + 4);
                }
                item.uid = static_cast<std::uint64_t>(std::stoull(line.substr(uid_marker + 4, uid_end - (uid_marker + 4))));
            }

            const std::size_t flags_marker = line.find("FLAGS (");
            if (flags_marker != std::string::npos) {
                const std::size_t flags_end = line.find(')', flags_marker + 7);
                if (flags_end != std::string::npos) {
                    item.flags = line.substr(flags_marker + 7, flags_end - (flags_marker + 7));
                }
            }

            const std::size_t literal_open = line.rfind('{');
            const std::size_t literal_close = line.rfind('}');
            if (literal_open != std::string::npos && literal_close != std::string::npos && literal_close > literal_open) {
                const std::size_t literal_size =
                    static_cast<std::size_t>(std::stoull(line.substr(literal_open + 1, literal_close - literal_open - 1)));
                item.raw_message.reserve(literal_size);
                while (item.raw_message.size() < literal_size) {
                    const std::string chunk =
                        connection_->Receive(literal_size - item.raw_message.size(), error_message);
                    if (chunk.empty()) {
                        return false;
                    }
                    item.raw_message += chunk;
                }
                connection_->ReceiveLine(&line, error_message);
            }
            items->push_back(std::move(item));
        }
        return false;
    }

    bool RunSimpleTagged(const std::string& command,
                         std::vector<std::string>* lines,
                         std::string* error_message) {
        const std::string tag = NextTag();
        if (!connection_->Send(tag + " " + command + "\r\n", error_message)) {
            return false;
        }
        return ReadTaggedResult(tag, lines, error_message);
    }

    bool ReadTaggedResult(const std::string& tag,
                          std::vector<std::string>* lines,
                          std::string* error_message) {
        std::string line;
        while (connection_->ReceiveLine(&line, error_message)) {
            if (StartsWithInsensitive(line, tag + " OK")) {
                return true;
            }
            if (StartsWithInsensitive(line, tag + " NO") || StartsWithInsensitive(line, tag + " BAD")) {
                if (error_message) {
                    *error_message = "IMAP command failed: " + line;
                }
                return false;
            }
            if (lines) {
                lines->push_back(line);
            }
        }
        return false;
    }

    std::string NextTag() {
        ++tag_counter_;
        return "A" + std::to_string(tag_counter_);
    }

    const AccountProfile& account_;
    std::string password_;
    TransportService& transport_service_;
    const TlsProvider& tls_provider_;
    std::unique_ptr<TransportConnection> connection_;
    int tag_counter_ = 0;
};

}  // namespace

MailTransportCoordinator::MailTransportCoordinator(AccountService& account_service,
                                                   CredentialStore& credential_store,
                                                   SyncStateStore& sync_state_store,
                                                   MailboxStore& mailbox_store,
                                                   MessageStore& message_store,
                                                   TransportService& transport_service,
                                                   TlsProvider& tls_provider,
                                                   MailTaskModel& task_model)
    : account_service_(account_service),
      credential_store_(credential_store),
      sync_state_store_(sync_state_store),
      mailbox_store_(mailbox_store),
      message_store_(message_store),
      transport_service_(transport_service),
      tls_provider_(tls_provider),
      task_model_(task_model) {}

MailTransportSummary MailTransportCoordinator::SendQueued() {
    MailTransportSummary summary;
    std::string ignored;
    mailbox_store_.EnsureMailbox({"sent", "Sent", {}, {}, MailboxProtocol::kLocal, {}, false, true, 0}, &ignored);

    const auto queued_messages = message_store_.ListMessages("out");
    const auto accounts = account_service_.Accounts();
    std::map<std::string, AccountProfile> account_by_id;
    for (const auto& account : accounts) {
        account_by_id.emplace(account.id, account);
    }
    if (accounts.empty()) {
        summary.error_message = "No accounts are configured.";
        return summary;
    }

    for (const auto& queued_message : queued_messages) {
        const AccountProfile& account =
            queued_message.account_id.empty() ? accounts.front() : account_by_id[queued_message.account_id];
        const auto password = credential_store_.LoadCredential(account.id, CredentialKind::kOutgoing);
        if (!password) {
            summary.error_message = "Missing outgoing credential for account " + account.id;
            continue;
        }

        TaskScope task(task_model_, MailTaskKind::kSending, DisplayName(account), "Send queued mail", "Connecting");
        SmtpSession smtp(account, *password, transport_service_, tls_provider_);

        MessageRecord updated = queued_message;
        updated.delivery_state = MessageDeliveryState::kSending;
        updated.updated_at = NowUnixSeconds();
        message_store_.SaveMessage(updated, nullptr);

        std::string error_message;
        if (!smtp.SendMessage(updated, &error_message)) {
            updated.delivery_state = MessageDeliveryState::kFailed;
            updated.last_error = error_message;
            updated.updated_at = NowUnixSeconds();
            message_store_.SaveMessage(updated, nullptr);
            task.Fail("Failed", error_message);
            summary.warnings.push_back(error_message);
            continue;
        }

        updated.delivery_state = MessageDeliveryState::kSent;
        updated.last_error.clear();
        updated.mailbox_id = "sent";
        updated.updated_at = NowUnixSeconds();
        if (message_store_.SaveMessage(updated, &error_message)) {
            message_store_.DeleteMessage("out", queued_message.id, nullptr);
        }
        ++summary.messages_sent;
        task.Complete("Sent");
    }

    summary.success = summary.error_message.empty();
    return summary;
}

MailTransportSummary MailTransportCoordinator::CheckMail() {
    MailTransportSummary summary;
    const auto accounts = account_service_.Accounts();
    if (accounts.empty()) {
        summary.error_message = "No accounts are configured.";
        return summary;
    }

    std::string ignored;
    mailbox_store_.EnsureMailbox({"inbox", "Inbox", {}, {}, MailboxProtocol::kLocal, {}, false, true, 0}, &ignored);

    for (const auto& account : accounts) {
        if (!account.check_mail_by_default && (account.uses_pop || account.uses_imap)) {
            continue;
        }

        if (account.uses_pop) {
            const auto password = credential_store_.LoadCredential(account.id, CredentialKind::kIncoming);
            if (!password) {
                summary.warnings.push_back("Missing incoming credential for account " + account.id);
                continue;
            }

            PopSyncState state = sync_state_store_.LoadPopState(account.id).value_or(PopSyncState{account.id, {}});
            std::vector<MessageRecord> messages;
            TaskScope task(task_model_, MailTaskKind::kReceiving, DisplayName(account), "Check POP mail", "Checking");
            std::string error_message;
            PopSession session(account, *password, transport_service_, tls_provider_);
            if (!session.FetchMessages(&state, &messages, &error_message)) {
                task.Fail("Failed", error_message);
                summary.warnings.push_back(error_message);
                continue;
            }

            sync_state_store_.SavePopState(state, nullptr);
            for (const auto& message : messages) {
                message_store_.SaveMessage(message, nullptr);
                ++summary.messages_received;
            }
            task.Complete("Complete");
        }

        if (account.uses_imap) {
            const auto password = credential_store_.LoadCredential(account.id, CredentialKind::kIncoming);
            if (!password) {
                summary.warnings.push_back("Missing incoming credential for account " + account.id);
                continue;
            }

            TaskScope discovery_task(
                task_model_, MailTaskKind::kMailboxDiscovery, DisplayName(account), "Refresh IMAP mailboxes", "Listing");
            std::string error_message;
            ImapSession session(account, *password, transport_service_, tls_provider_);
            std::vector<std::string> remote_mailboxes;
            if (!session.DiscoverMailboxes(&remote_mailboxes, &error_message)) {
                discovery_task.Fail("Failed", error_message);
                summary.warnings.push_back(error_message);
                continue;
            }
            discovery_task.Complete("Listed");

            summary.mailboxes_discovered += remote_mailboxes.size();
            for (const auto& remote_mailbox : remote_mailboxes) {
                const std::string local_mailbox_id = LocalMailboxIdForImap(account.id, remote_mailbox);
                mailbox_store_.EnsureMailbox(
                    {local_mailbox_id,
                     remote_mailbox,
                     {},
                     account.id,
                     MailboxProtocol::kImap,
                     remote_mailbox,
                     true,
                     false,
                     0},
                    nullptr);

                ImapMailboxSyncState state =
                    sync_state_store_.LoadImapState(account.id, local_mailbox_id)
                        .value_or(ImapMailboxSyncState{account.id, local_mailbox_id, 0, 0});
                std::vector<MessageRecord> messages;
                TaskScope sync_task(
                    task_model_, MailTaskKind::kImapSync, DisplayName(account), "Sync " + remote_mailbox, "Syncing");
                ImapSession sync_session(account, *password, transport_service_, tls_provider_);
                if (!sync_session.SyncMailbox(remote_mailbox, &state, &messages, &error_message)) {
                    sync_task.Fail("Failed", error_message);
                    summary.warnings.push_back(error_message);
                    continue;
                }
                sync_state_store_.SaveImapState(state, nullptr);
                for (const auto& message : messages) {
                    message_store_.SaveMessage(message, nullptr);
                    ++summary.messages_received;
                }
                sync_task.Complete("Complete");
            }
        }
    }

    summary.success = summary.error_message.empty();
    return summary;
}

MailTransportSummary MailTransportCoordinator::SendAndReceive() {
    MailTransportSummary send_summary = SendQueued();
    MailTransportSummary receive_summary = CheckMail();

    MailTransportSummary combined;
    combined.success = send_summary.success && receive_summary.success;
    combined.messages_sent = send_summary.messages_sent;
    combined.messages_received = receive_summary.messages_received;
    combined.mailboxes_discovered = receive_summary.mailboxes_discovered;
    combined.warnings = send_summary.warnings;
    combined.warnings.insert(combined.warnings.end(), receive_summary.warnings.begin(), receive_summary.warnings.end());
    if (!send_summary.error_message.empty()) {
        combined.error_message = send_summary.error_message;
    } else {
        combined.error_message = receive_summary.error_message;
    }
    return combined;
}

}  // namespace hermes
