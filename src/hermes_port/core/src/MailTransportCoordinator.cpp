#include "hermes/MailTransportCoordinator.h"
#include "hermes/GssapiAuthenticator.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
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
    std::vector<std::string> attachment_payloads;
};

struct BuiltReceivedMessage {
    MessageRecord record;
    std::vector<std::string> attachment_payloads;
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

std::string WrapBase64(std::string encoded, std::size_t line_length = 76) {
    if (encoded.size() <= line_length) {
        return encoded;
    }
    std::string wrapped;
    wrapped.reserve(encoded.size() + (encoded.size() / line_length) * 2);
    for (std::size_t offset = 0; offset < encoded.size(); offset += line_length) {
        wrapped.append(encoded, offset, std::min(line_length, encoded.size() - offset));
        wrapped += "\r\n";
    }
    return wrapped;
}

int HexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    return -1;
}

std::string DecodeQuotedPrintable(const std::string& input) {
    std::string decoded;
    decoded.reserve(input.size());
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (input[index] != '=') {
            decoded.push_back(input[index]);
            continue;
        }
        if (index + 2 < input.size() && input[index + 1] == '\r' && input[index + 2] == '\n') {
            index += 2;
            continue;
        }
        if (index + 1 < input.size() && input[index + 1] == '\n') {
            ++index;
            continue;
        }
        if (index + 2 < input.size()) {
            const int high = HexValue(input[index + 1]);
            const int low = HexValue(input[index + 2]);
            if (high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>((high << 4) | low));
                index += 2;
                continue;
            }
        }
        decoded.push_back('=');
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

std::string StripAngleBrackets(std::string value) {
    value = Trim(std::move(value));
    if (value.size() >= 2 && value.front() == '<' && value.back() == '>') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string DispositionToken(std::string value) {
    value = ToLower(Trim(std::move(value)));
    const std::size_t semicolon = value.find(';');
    if (semicolon != std::string::npos) {
        value.erase(semicolon);
    }
    return value.empty() ? "attachment" : value;
}

std::string DecodeTransferEncodedBody(const std::map<std::string, std::string>& headers,
                                      std::string body) {
    const auto encoding_it = headers.find("content-transfer-encoding");
    if (encoding_it == headers.end()) {
        return body;
    }
    const std::string encoding = ToLower(encoding_it->second);
    if (encoding.find("base64") != std::string::npos) {
        return Base64Decode(body);
    }
    if (encoding.find("quoted-printable") != std::string::npos) {
        return DecodeQuotedPrintable(body);
    }
    return body;
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
        const std::string decoded_part_body = DecodeTransferEncodedBody(headers, part_body);

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
            attachment.size = decoded_part_body.size();
            attachment.content_id =
                StripAngleBrackets(headers.count("content-id") ? headers.at("content-id") : "");
            attachment.disposition = DispositionToken(headers.count("content-disposition")
                                                          ? headers.at("content-disposition")
                                                          : "attachment");
            attachment.download_complete = true;
            parsed.attachments.push_back(std::move(attachment));
            parsed.attachment_payloads.push_back(decoded_part_body);
            continue;
        }

        if (part_type.find("text/html") != std::string::npos) {
            parsed.html_body = decoded_part_body;
        } else if (part_type.find("text/plain") != std::string::npos) {
            parsed.plain_text_body = decoded_part_body;
        }
    }

    if (parsed.plain_text_body.empty() && !parsed.html_body.empty()) {
        parsed.plain_text_body = parsed.html_body;
    }
    return parsed;
}

BuiltReceivedMessage BuildReceivedMessage(const std::string& raw_message,
                                         const AccountProfile& account,
                                         std::string_view mailbox_id,
                                         std::string_view remote_id,
                                         std::string_view remote_mailbox,
                                         bool unread,
                                         bool download_complete,
                                         bool flagged = false,
                                         bool deleted = false,
                                         bool answered = false,
                                         bool honor_download_limits = true,
                                         bool honor_attachment_omit = true) {
    const ParsedMimeMessage parsed = ParseMimeMessage(raw_message);
    BuiltReceivedMessage built;
    MessageRecord& record = built.record;
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
    record.attachments_omitted =
        honor_attachment_omit && account.imap_omit_attachments && !record.attachments.empty();
    record.flagged = flagged;
    record.deleted = deleted;
    record.answered = answered;
    record.unread = unread;
    record.created_at = NowUnixSeconds();
    record.updated_at = record.created_at;

    if (honor_download_limits && account.imap_max_download_size > 0 &&
        record.plain_text_body.size() > account.imap_max_download_size) {
        record.plain_text_body.resize(account.imap_max_download_size);
        record.download_complete = false;
    }
    built.attachment_payloads = parsed.attachment_payloads;
    if (honor_attachment_omit && account.imap_omit_attachments) {
        for (auto& attachment : record.attachments) {
            attachment.omitted = true;
            attachment.download_complete = false;
        }
        built.attachment_payloads.clear();
    }
    return built;
}

std::string ReadAttachmentBytes(const MessageAttachment& attachment) {
    if (attachment.payload_path.empty()) {
        return {};
    }
    std::ifstream input(attachment.payload_path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string BuildBodyPart(const MessageRecord& message) {
    std::ostringstream output;
    if (!message.html_body.empty()) {
        output << "Content-Type: multipart/alternative; boundary=\"hermes-alternative\"\r\n\r\n";
        output << "--hermes-alternative\r\n";
        output << "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
        output << message.plain_text_body << "\r\n";
        output << "--hermes-alternative\r\n";
        output << "Content-Type: text/html; charset=UTF-8\r\n\r\n";
        output << message.html_body << "\r\n";
        output << "--hermes-alternative--\r\n";
    } else {
        output << "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
        output << message.plain_text_body << "\r\n";
    }
    return output.str();
}

std::string BuildSmtpMessage(const MessageRecord& message) {
    std::ostringstream output;
    output << "Subject: " << message.subject << "\r\n";
    output << "From: " << message.sender << "\r\n";
    output << "To: " << message.recipients << "\r\n";
    output << "MIME-Version: 1.0\r\n";

    if (!message.attachments.empty()) {
        output << "Content-Type: multipart/mixed; boundary=\"hermes-mixed\"\r\n\r\n";
        output << "--hermes-mixed\r\n";
        output << BuildBodyPart(message);
        for (const auto& attachment : message.attachments) {
            const std::string payload = ReadAttachmentBytes(attachment);
            output << "--hermes-mixed\r\n";
            output << "Content-Type: "
                   << (attachment.content_type.empty() ? "application/octet-stream" : attachment.content_type);
            if (!attachment.name.empty()) {
                output << "; name=\"" << attachment.name << "\"";
            }
            output << "\r\n";
            output << "Content-Disposition: "
                   << (attachment.disposition.empty() ? "attachment" : attachment.disposition);
            if (!attachment.name.empty()) {
                output << "; filename=\"" << attachment.name << "\"";
            }
            output << "\r\n";
            if (!attachment.content_id.empty()) {
                output << "Content-ID: <" << attachment.content_id << ">\r\n";
            }
            output << "Content-Transfer-Encoding: base64\r\n\r\n";
            output << WrapBase64(Base64Encode(payload)) << "\r\n";
        }
        output << "--hermes-mixed--\r\n";
        return output.str();
    }

    output << BuildBodyPart(message);
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

bool PersistAttachmentPayloads(MessageStore& message_store,
                               const MessageRecord& record,
                               const std::vector<std::string>& payloads,
                               std::string* error_message) {
    for (std::size_t index = 0; index < payloads.size(); ++index) {
        const std::string suggested_name =
            index < record.attachments.size() && !record.attachments[index].name.empty()
                ? record.attachments[index].name
                : "attachment.bin";
        if (!message_store.SaveAttachmentPayload(record.mailbox_id,
                                                 record.id,
                                                 index,
                                                 suggested_name,
                                                 payloads[index],
                                                 error_message)) {
            return false;
        }
    }
    return true;
}

struct AuthFailureDetails {
    MailTaskErrorKind kind = MailTaskErrorKind::kUnknown;
    std::string mechanism;
    std::string message;
};

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

    void Fail(std::string status,
              std::string error_message,
              MailTaskErrorKind kind = MailTaskErrorKind::kUnknown,
              std::string mechanism = {}) {
        task_model_.FailTask(task_.id, status, error_message, kind, mechanism);
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
               MessageStore& message_store,
               TransportService& transport_service,
               const TlsProvider& tls_provider,
               const GssapiEngine& gssapi_engine)
        : account_(account),
          password_(std::move(password)),
          message_store_(message_store),
          transport_service_(transport_service),
          tls_provider_(tls_provider),
          gssapi_engine_(gssapi_engine) {}

    bool FetchMessages(PopSyncState* state,
                       std::vector<MessageRecord>* messages,
                       std::string* error_message) {
        last_auth_failure_.reset();
        const std::uint16_t incoming_port =
            account_.pop_auth == PopAuthMode::kKerberos && account_.kerberos.pop_port != 0
                ? account_.kerberos.pop_port
                : account_.incoming_port;
        auto connection = transport_service_.Connect(
            {account_.incoming_server, incoming_port, MapSecurity(account_.incoming_security), 5000},
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

            BuiltReceivedMessage built =
                BuildReceivedMessage(raw_message, account_, "inbox", entry.second, "INBOX", true, true);
            MessageRecord record = std::move(built.record);
            record.id = PopMessageId(account_.id, entry.second);
            if (messages) {
                messages->push_back(record);
            }
            if (state) {
                state->uidl_to_message_id[entry.second] = record.id;
            }
            if (!PersistAttachmentPayloads(message_store_, record, built.attachment_payloads, error_message)) {
                return false;
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

    const std::optional<AuthFailureDetails>& LastAuthFailure() const {
        return last_auth_failure_;
    }

private:
    bool Authenticate(TransportConnection& connection,
                      const std::string& greeting,
                      std::string* error_message) {
        last_auth_failure_.reset();
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

            case PopAuthMode::kKerberos: {
                MailTaskErrorKind principal_error_kind = MailTaskErrorKind::kUnknown;
                std::string principal_error_message;
                const std::string service_principal = BuildKerberosServicePrincipal(
                    account_,
                    account_.incoming_server,
                    "pop",
                    &principal_error_kind,
                    &principal_error_message);
                if (service_principal.empty()) {
                    last_auth_failure_ =
                        AuthFailureDetails{principal_error_kind, "GSSAPI", std::move(principal_error_message)};
                    if (error_message) {
                        *error_message = last_auth_failure_->message;
                    }
                    return false;
                }

                if (!connection.Send("AUTH GSSAPI\r\n", error_message)) {
                    return false;
                }
                GssapiAuthenticator authenticator(gssapi_engine_);
                GssapiAuthFailure auth_failure;
                if (!authenticator.Exchange(
                        service_principal,
                        account_.login_name,
                        [&](std::string* challenge, std::string* challenge_error) {
                            std::string line;
                            if (!connection.ReceiveLine(&line, challenge_error)) {
                                return false;
                            }
                            if (line.empty() || line[0] != '+') {
                                if (challenge_error) {
                                    *challenge_error = "POP GSSAPI challenge was not received.";
                                }
                                return false;
                            }
                            *challenge = line.size() > 1 ? Trim(line.substr(1)) : "";
                            return true;
                        },
                        [&](std::string_view response, std::string* send_error) {
                            return connection.Send(std::string(response), send_error);
                        },
                        &auth_failure)) {
                    std::string message = auth_failure.message;
                    if (!auth_failure.principal.empty()) {
                        message += " [principal: " + auth_failure.principal + "]";
                    }
                    last_auth_failure_ =
                        AuthFailureDetails{auth_failure.kind, auth_failure.mechanism, std::move(message)};
                    if (error_message) {
                        *error_message = last_auth_failure_->message;
                    }
                    return false;
                }
                {
                    std::string line;
                    if (!connection.ReceiveLine(&line, error_message)) {
                        return false;
                    }
                    if (line.rfind("+OK", 0) != 0) {
                        last_auth_failure_ =
                            AuthFailureDetails{MailTaskErrorKind::kServerRejected,
                                               "GSSAPI",
                                               "POP GSSAPI authentication failed: " + line};
                        if (error_message) {
                            *error_message = last_auth_failure_->message;
                        }
                        return false;
                    }
                    last_auth_failure_.reset();
                    return true;
                }
            }

            case PopAuthMode::kRPA:
                last_auth_failure_ = AuthFailureDetails{
                    MailTaskErrorKind::kUnsupportedMechanism,
                    "RPA",
                    "POP RPA authentication is not yet implemented.",
                };
                if (error_message) {
                    *error_message = last_auth_failure_->message;
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
    MessageStore& message_store_;
    TransportService& transport_service_;
    const TlsProvider& tls_provider_;
    const GssapiEngine& gssapi_engine_;
    std::optional<AuthFailureDetails> last_auth_failure_;
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
                MessageStore& message_store,
                TransportService& transport_service,
                const TlsProvider& tls_provider,
                const GssapiEngine& gssapi_engine)
        : account_(account),
          password_(std::move(password)),
          message_store_(message_store),
          transport_service_(transport_service),
          tls_provider_(tls_provider),
          gssapi_engine_(gssapi_engine) {}

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
        if (!SelectMailbox(remote_mailbox, &select_lines, error_message)) {
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
            BuiltReceivedMessage built = BuildReceivedMessage(item.raw_message,
                                                             account_,
                                                             local_mailbox_id,
                                                             std::to_string(item.uid),
                                                             remote_mailbox,
                                                             item.flags.find("\\Seen") == std::string::npos,
                                                             true,
                                                             item.flags.find("\\Flagged") != std::string::npos,
                                                             item.flags.find("\\Deleted") != std::string::npos,
                                                             item.flags.find("\\Answered") != std::string::npos);
            MessageRecord record = std::move(built.record);
            record.id = ImapMessageId(account_.id, local_mailbox_id, item.uid);
            if (!PersistAttachmentPayloads(message_store_, record, built.attachment_payloads, error_message)) {
                return false;
            }
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

    bool CreateMailbox(std::string_view remote_mailbox, std::string* error_message) {
        if (!ConnectAndAuthenticate(error_message)) {
            return false;
        }
        return RunSimpleTagged("CREATE \"" + std::string(remote_mailbox) + "\"", nullptr, error_message);
    }

    bool RenameMailbox(std::string_view remote_mailbox,
                       std::string_view new_remote_mailbox,
                       std::string* error_message) {
        if (!ConnectAndAuthenticate(error_message)) {
            return false;
        }
        return RunSimpleTagged("RENAME \"" + std::string(remote_mailbox) + "\" \"" +
                                   std::string(new_remote_mailbox) + "\"",
                               nullptr,
                               error_message);
    }

    bool DeleteMailbox(std::string_view remote_mailbox, std::string* error_message) {
        if (!ConnectAndAuthenticate(error_message)) {
            return false;
        }
        return RunSimpleTagged("DELETE \"" + std::string(remote_mailbox) + "\"", nullptr, error_message);
    }

    bool CopyMessage(std::string_view remote_mailbox,
                     std::string_view remote_message_id,
                     std::string_view destination_remote_mailbox,
                     std::string* error_message) {
        if (!ConnectAndAuthenticate(error_message)) {
            return false;
        }
        std::vector<std::string> ignored;
        if (!SelectMailbox(remote_mailbox, &ignored, error_message)) {
            return false;
        }
        return RunSimpleTagged("UID COPY " + std::string(remote_message_id) + " \"" +
                                   std::string(destination_remote_mailbox) + "\"",
                               nullptr,
                               error_message);
    }

    bool SetDeleted(std::string_view remote_mailbox,
                    std::string_view remote_message_id,
                    bool deleted,
                    bool expunge,
                    std::string* error_message) {
        if (!ConnectAndAuthenticate(error_message)) {
            return false;
        }
        std::vector<std::string> ignored;
        if (!SelectMailbox(remote_mailbox, &ignored, error_message)) {
            return false;
        }
        const std::string op = deleted ? "+FLAGS.SILENT" : "-FLAGS.SILENT";
        if (!RunSimpleTagged("UID STORE " + std::string(remote_message_id) + " " + op + " (\\Deleted)",
                             nullptr,
                             error_message)) {
            return false;
        }
        if (deleted && expunge) {
            return RunSimpleTagged("EXPUNGE", nullptr, error_message);
        }
        return true;
    }

    bool MoveMessage(std::string_view remote_mailbox,
                     std::string_view remote_message_id,
                     std::string_view destination_remote_mailbox,
                     std::string* error_message) {
        if (!ConnectAndAuthenticate(error_message)) {
            return false;
        }
        std::vector<std::string> ignored;
        if (!SelectMailbox(remote_mailbox, &ignored, error_message)) {
            return false;
        }
        std::string move_error;
        if (RunSimpleTagged("UID MOVE " + std::string(remote_message_id) + " \"" +
                                std::string(destination_remote_mailbox) + "\"",
                            nullptr,
                            &move_error)) {
            return true;
        }
        if (!CopyMessage(remote_mailbox, remote_message_id, destination_remote_mailbox, error_message)) {
            return false;
        }
        return SetDeleted(remote_mailbox, remote_message_id, true, true, error_message);
    }

    bool FetchFullMessage(std::string_view remote_mailbox,
                          std::string_view remote_message_id,
                          MessageRecord* record,
                          std::vector<std::string>* attachment_payloads,
                          std::string* error_message) {
        if (!ConnectAndAuthenticate(error_message)) {
            return false;
        }
        std::vector<std::string> ignored;
        if (!SelectMailbox(remote_mailbox, &ignored, error_message)) {
            return false;
        }
        std::vector<ImapFetchItem> items;
        if (!FetchItemsByRange(std::string(remote_message_id) + ":" + std::string(remote_message_id),
                               "BODY.PEEK[]",
                               &items,
                               error_message)) {
            return false;
        }
        if (items.empty()) {
            if (error_message) {
                *error_message = "IMAP message was not returned by the server.";
            }
            return false;
        }
        const std::string local_mailbox_id = LocalMailboxIdForImap(account_.id, remote_mailbox);
        BuiltReceivedMessage built = BuildReceivedMessage(items.front().raw_message,
                                                         account_,
                                                         local_mailbox_id,
                                                         std::string(remote_message_id),
                                                         remote_mailbox,
                                                         items.front().flags.find("\\Seen") == std::string::npos,
                                                         true,
                                                         items.front().flags.find("\\Flagged") != std::string::npos,
                                                         items.front().flags.find("\\Deleted") != std::string::npos,
                                                         items.front().flags.find("\\Answered") != std::string::npos,
                                                         false,
                                                         false);
        if (record) {
            *record = std::move(built.record);
            record->id = ImapMessageId(account_.id,
                                       local_mailbox_id,
                                       static_cast<std::uint64_t>(std::stoull(std::string(remote_message_id))));
        }
        if (attachment_payloads) {
            *attachment_payloads = std::move(built.attachment_payloads);
        }
        return true;
    }

    const std::optional<AuthFailureDetails>& LastAuthFailure() const {
        return last_auth_failure_;
    }

private:
    bool ConnectAndAuthenticate(std::string* error_message) {
        if (connection_) {
            return true;
        }
        last_auth_failure_.reset();
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

            case ImapAuthMode::kKerberos: {
                MailTaskErrorKind principal_error_kind = MailTaskErrorKind::kUnknown;
                std::string principal_error_message;
                const std::string service_principal = BuildKerberosServicePrincipal(
                    account_,
                    account_.incoming_server,
                    "imap",
                    &principal_error_kind,
                    &principal_error_message);
                if (service_principal.empty()) {
                    last_auth_failure_ =
                        AuthFailureDetails{principal_error_kind, "GSSAPI", std::move(principal_error_message)};
                    if (error_message) {
                        *error_message = last_auth_failure_->message;
                    }
                    return false;
                }

                const std::string tag = NextTag();
                if (!connection_->Send(tag + " AUTHENTICATE GSSAPI\r\n", error_message)) {
                    return false;
                }
                GssapiAuthenticator authenticator(gssapi_engine_);
                GssapiAuthFailure auth_failure;
                if (!authenticator.Exchange(
                        service_principal,
                        account_.login_name,
                        [&](std::string* challenge, std::string* challenge_error) {
                            std::string line;
                            if (!connection_->ReceiveLine(&line, challenge_error)) {
                                return false;
                            }
                            if (line.empty() || line[0] != '+') {
                                if (challenge_error) {
                                    *challenge_error = "IMAP GSSAPI challenge was not received.";
                                }
                                return false;
                            }
                            *challenge = line.size() > 1 ? Trim(line.substr(1)) : "";
                            return true;
                        },
                        [&](std::string_view response, std::string* send_error) {
                            return connection_->Send(std::string(response), send_error);
                        },
                        &auth_failure)) {
                    std::string message = auth_failure.message;
                    if (!auth_failure.principal.empty()) {
                        message += " [principal: " + auth_failure.principal + "]";
                    }
                    last_auth_failure_ =
                        AuthFailureDetails{auth_failure.kind, auth_failure.mechanism, std::move(message)};
                    if (error_message) {
                        *error_message = last_auth_failure_->message;
                    }
                    return false;
                }
                if (!ReadTaggedResult(tag, nullptr, error_message)) {
                    if (error_message && StartsWithInsensitive(*error_message, "IMAP command failed:")) {
                        last_auth_failure_ = AuthFailureDetails{
                            MailTaskErrorKind::kServerRejected,
                            "GSSAPI",
                            *error_message,
                        };
                    }
                    return false;
                }
                last_auth_failure_.reset();
                return true;
            }
        }
        return false;
    }

    bool FetchItems(std::uint64_t next_uid, std::vector<ImapFetchItem>* items, std::string* error_message) {
        const char* body_selector =
            account_.imap_download_mode == ImapDownloadMode::kMinimalHeaders ? "BODY.PEEK[HEADER]"
                                                                             : "BODY.PEEK[]";
        return FetchItemsByRange(std::to_string(next_uid) + ":*", body_selector, items, error_message);
    }

    bool FetchItemsByRange(std::string_view uid_range,
                           std::string_view body_selector,
                           std::vector<ImapFetchItem>* items,
                           std::string* error_message) {
        const std::string tag = NextTag();
        if (!connection_->Send(tag + " UID FETCH " + std::string(uid_range) + " (UID FLAGS " +
                                   std::string(body_selector) + ")\r\n",
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

    bool SelectMailbox(std::string_view remote_mailbox,
                       std::vector<std::string>* select_lines,
                       std::string* error_message) {
        return RunSimpleTagged("SELECT \"" + std::string(remote_mailbox) + "\"", select_lines, error_message);
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
    MessageStore& message_store_;
    TransportService& transport_service_;
    const TlsProvider& tls_provider_;
    const GssapiEngine& gssapi_engine_;
    std::unique_ptr<TransportConnection> connection_;
    std::optional<AuthFailureDetails> last_auth_failure_;
    int tag_counter_ = 0;
};

std::optional<AccountProfile> FindAccountById(const AccountService& account_service, std::string_view account_id) {
    return account_service.FindById(account_id);
}

bool EnsureRemoteMailbox(MailboxStore& mailbox_store,
                         const AccountProfile& account,
                         std::string_view remote_name,
                         std::string* error_message) {
    MailboxRecord mailbox;
    mailbox.id = LocalMailboxIdForImap(account.id, remote_name);
    mailbox.display_name = std::string(remote_name);
    mailbox.account_id = account.id;
    mailbox.protocol = MailboxProtocol::kImap;
    mailbox.remote_name = std::string(remote_name);
    mailbox.is_remote = true;
    return mailbox_store.EnsureMailbox(mailbox, error_message);
}

bool SaveUpdatedMessage(MessageStore& message_store,
                        std::string_view mailbox_id,
                        std::string_view message_id,
                        const std::function<void(MessageRecord&)>& updater,
                        std::string* error_message) {
    const auto record = message_store.GetMessage(mailbox_id, message_id);
    if (!record) {
        if (error_message) {
            *error_message = "Message not found: " + std::string(message_id);
        }
        return false;
    }
    MessageRecord updated = *record;
    updater(updated);
    return message_store.SaveMessage(updated, error_message);
}

bool OptimisticallyMoveMessage(MessageStore& message_store,
                               MailboxStore& mailbox_store,
                               const AccountProfile& account,
                               std::string_view source_mailbox_id,
                               std::string_view message_id,
                               std::string_view destination_remote_mailbox,
                               std::string* error_message) {
    if (!EnsureRemoteMailbox(mailbox_store, account, destination_remote_mailbox, error_message)) {
        return false;
    }
    const std::string destination_mailbox_id = LocalMailboxIdForImap(account.id, destination_remote_mailbox);
    const auto record = message_store.GetMessage(source_mailbox_id, message_id);
    if (!record) {
        if (error_message) {
            *error_message = "Message not found: " + std::string(message_id);
        }
        return false;
    }
    MessageRecord updated = *record;
    updated.mailbox_id = destination_mailbox_id;
    updated.remote_mailbox = std::string(destination_remote_mailbox);
    updated.deleted = false;
    if (!message_store.SaveMessage(updated, error_message)) {
        return false;
    }
    return message_store.DeleteMessage(source_mailbox_id, message_id, error_message);
}

bool OptimisticallyCopyMessage(MessageStore& message_store,
                               MailboxStore& mailbox_store,
                               const AccountProfile& account,
                               std::string_view source_mailbox_id,
                               std::string_view message_id,
                               std::string_view destination_remote_mailbox,
                               std::string* error_message) {
    if (!EnsureRemoteMailbox(mailbox_store, account, destination_remote_mailbox, error_message)) {
        return false;
    }
    const auto record = message_store.GetMessage(source_mailbox_id, message_id);
    if (!record) {
        if (error_message) {
            *error_message = "Message not found: " + std::string(message_id);
        }
        return false;
    }
    MessageRecord updated = *record;
    updated.mailbox_id = LocalMailboxIdForImap(account.id, destination_remote_mailbox);
    updated.remote_mailbox = std::string(destination_remote_mailbox);
    return message_store.SaveMessage(updated, error_message);
}

bool RenameMailboxMessages(MessageStore& message_store,
                           std::string_view source_mailbox_id,
                           std::string_view destination_mailbox_id,
                           std::string_view destination_remote_mailbox,
                           std::string* error_message) {
    const auto messages = message_store.ListMessages(source_mailbox_id);
    for (const auto& message : messages) {
        MessageRecord updated = message;
        updated.mailbox_id = std::string(destination_mailbox_id);
        updated.remote_mailbox = std::string(destination_remote_mailbox);
        if (!message_store.SaveMessage(updated, error_message)) {
            return false;
        }
        if (!message_store.DeleteMessage(source_mailbox_id, message.id, error_message)) {
            return false;
        }
    }
    return true;
}

bool DeleteMailboxMessages(MessageStore& message_store,
                           std::string_view mailbox_id,
                           std::string* error_message) {
    const auto messages = message_store.ListMessages(mailbox_id);
    for (const auto& message : messages) {
        if (!message_store.DeleteMessage(mailbox_id, message.id, error_message)) {
            return false;
        }
    }
    return true;
}

bool ReplaySingleImapAction(const ImapActionRecord& action,
                            AccountService& account_service,
                            CredentialStore& credential_store,
                            SyncStateStore& sync_state_store,
                            MailboxStore& mailbox_store,
                            MessageStore& message_store,
                            TransportService& transport_service,
                            TlsProvider& tls_provider,
                            const GssapiEngine& gssapi_engine,
                            std::optional<AuthFailureDetails>* auth_failure,
                            std::string* error_message) {
    if (auth_failure) {
        auth_failure->reset();
    }
    const auto account = FindAccountById(account_service, action.account_id);
    if (!account) {
        if (error_message) {
            *error_message = "Unable to find account for IMAP action.";
        }
        return false;
    }

    const auto password = credential_store.LoadCredential(account->id, CredentialKind::kIncoming);
    if (!password) {
        if (error_message) {
            *error_message = "Missing incoming credential for account " + account->id;
        }
        return false;
    }

    ImapSession session(*account, *password, message_store, transport_service, tls_provider, gssapi_engine);
    switch (action.kind) {
        case ImapActionKind::kDelete:
            if (!action.destination_remote_mailbox.empty()) {
                const bool ok = session.MoveMessage(action.remote_mailbox,
                                                    action.remote_message_id,
                                                    action.destination_remote_mailbox,
                                                    error_message);
                if (!ok && auth_failure && session.LastAuthFailure()) {
                    *auth_failure = session.LastAuthFailure();
                }
                return ok;
            }
            {
                const bool ok = session.SetDeleted(action.remote_mailbox,
                                                   action.remote_message_id,
                                                   true,
                                                   !account->mark_as_deleted,
                                                   error_message);
                if (!ok && auth_failure && session.LastAuthFailure()) {
                    *auth_failure = session.LastAuthFailure();
                }
                return ok;
            }

        case ImapActionKind::kUndelete:
            {
                const bool ok = session.SetDeleted(action.remote_mailbox,
                                                   action.remote_message_id,
                                                   false,
                                                   false,
                                                   error_message);
                if (!ok && auth_failure && session.LastAuthFailure()) {
                    *auth_failure = session.LastAuthFailure();
                }
                return ok;
            }

        case ImapActionKind::kMove:
            {
                const bool ok = session.MoveMessage(action.remote_mailbox,
                                                    action.remote_message_id,
                                                    action.destination_remote_mailbox,
                                                    error_message);
                if (!ok && auth_failure && session.LastAuthFailure()) {
                    *auth_failure = session.LastAuthFailure();
                }
                return ok;
            }

        case ImapActionKind::kCopy:
            {
                const bool ok = session.CopyMessage(action.remote_mailbox,
                                                    action.remote_message_id,
                                                    action.destination_remote_mailbox,
                                                    error_message);
                if (!ok && auth_failure && session.LastAuthFailure()) {
                    *auth_failure = session.LastAuthFailure();
                }
                return ok;
            }

        case ImapActionKind::kCreateMailbox:
            if (!session.CreateMailbox(action.remote_mailbox, error_message)) {
                if (auth_failure && session.LastAuthFailure()) {
                    *auth_failure = session.LastAuthFailure();
                }
                return false;
            }
            return EnsureRemoteMailbox(mailbox_store, *account, action.remote_mailbox, error_message);

        case ImapActionKind::kRenameMailbox:
            if (!session.RenameMailbox(action.remote_mailbox, action.rename_target, error_message)) {
                if (auth_failure && session.LastAuthFailure()) {
                    *auth_failure = session.LastAuthFailure();
                }
                return false;
            }
            {
                const std::string renamed_mailbox_id = LocalMailboxIdForImap(account->id, action.rename_target);
                if (mailbox_store.GetMailbox(renamed_mailbox_id).has_value()) {
                    if (const auto existing = mailbox_store.GetMailbox(renamed_mailbox_id)) {
                        MailboxRecord updated = *existing;
                        updated.display_name = action.rename_target;
                        updated.remote_name = action.rename_target;
                        if (!mailbox_store.EnsureMailbox(updated, error_message)) {
                            return false;
                        }
                    }
                    return true;
                }
                if (mailbox_store.GetMailbox(action.mailbox_id).has_value()) {
                    if (!mailbox_store.RenameMailbox(action.mailbox_id,
                                                     renamed_mailbox_id,
                                                     action.rename_target,
                                                     error_message)) {
                        return false;
                    }
                    return RenameMailboxMessages(message_store,
                                                 action.mailbox_id,
                                                 renamed_mailbox_id,
                                                 action.rename_target,
                                                 error_message);
                }
                return EnsureRemoteMailbox(mailbox_store, *account, action.rename_target, error_message);
            }

        case ImapActionKind::kDeleteMailbox:
            if (!session.DeleteMailbox(action.remote_mailbox, error_message)) {
                if (auth_failure && session.LastAuthFailure()) {
                    *auth_failure = session.LastAuthFailure();
                }
                return false;
            }
            if (mailbox_store.GetMailbox(action.mailbox_id).has_value()) {
                if (!DeleteMailboxMessages(message_store, action.mailbox_id, error_message)) {
                    return false;
                }
            }
            return mailbox_store.DeleteMailbox(action.mailbox_id, error_message);

        case ImapActionKind::kFetchAttachment:
        case ImapActionKind::kFetchFullMessage: {
            MessageRecord record;
            std::vector<std::string> payloads;
            if (!session.FetchFullMessage(action.remote_mailbox,
                                          action.remote_message_id,
                                          &record,
                                          &payloads,
                                          error_message)) {
                if (auth_failure && session.LastAuthFailure()) {
                    *auth_failure = session.LastAuthFailure();
                }
                return false;
            }
            if (!message_store.SaveMessage(record, error_message)) {
                return false;
            }
            return PersistAttachmentPayloads(message_store, record, payloads, error_message);
        }

        case ImapActionKind::kResyncMailbox: {
            ImapMailboxSyncState state =
                sync_state_store.LoadImapState(account->id, action.mailbox_id)
                    .value_or(ImapMailboxSyncState{account->id, action.mailbox_id, 0, 0});
            state.last_seen_uid = 0;
            std::vector<MessageRecord> messages;
            if (!session.SyncMailbox(action.remote_mailbox, &state, &messages, error_message)) {
                if (auth_failure && session.LastAuthFailure()) {
                    *auth_failure = session.LastAuthFailure();
                }
                return false;
            }
            if (!sync_state_store.SaveImapState(state, error_message)) {
                return false;
            }
            for (const auto& message : messages) {
                if (!message_store.SaveMessage(message, error_message)) {
                    return false;
                }
            }
            return true;
        }

        case ImapActionKind::kRefreshMailboxList: {
            std::vector<std::string> remote_mailboxes;
            if (!session.DiscoverMailboxes(&remote_mailboxes, error_message)) {
                if (auth_failure && session.LastAuthFailure()) {
                    *auth_failure = session.LastAuthFailure();
                }
                return false;
            }
            for (const auto& remote_mailbox : remote_mailboxes) {
                if (!EnsureRemoteMailbox(mailbox_store, *account, remote_mailbox, error_message)) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

}  // namespace

MailTransportCoordinator::MailTransportCoordinator(AccountService& account_service,
                                                   CredentialStore& credential_store,
                                                   SyncStateStore& sync_state_store,
                                                   MailboxStore& mailbox_store,
                                                   MessageStore& message_store,
                                                   TransportService& transport_service,
                                                   TlsProvider& tls_provider,
                                                   MailTaskModel& task_model,
                                                   ImapActionStore* imap_action_store,
                                                   const GssapiEngine* gssapi_engine)
    : account_service_(account_service),
      credential_store_(credential_store),
      sync_state_store_(sync_state_store),
      mailbox_store_(mailbox_store),
      message_store_(message_store),
      transport_service_(transport_service),
      tls_provider_(tls_provider),
      task_model_(task_model),
      imap_action_store_(imap_action_store),
      gssapi_engine_(gssapi_engine != nullptr ? gssapi_engine : &DefaultGssapiEngine()) {}

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
    stop_requested_ = false;
    const auto accounts = account_service_.Accounts();
    if (accounts.empty()) {
        summary.error_message = "No accounts are configured.";
        return summary;
    }

    std::string ignored;
    mailbox_store_.EnsureMailbox({"inbox", "Inbox", {}, {}, MailboxProtocol::kLocal, {}, false, true, 0}, &ignored);

    if (imap_action_store_) {
        for (auto action : imap_action_store_->ListActions()) {
            if (stop_requested_) {
                summary.warnings.push_back("IMAP action replay stopped by user request.");
                break;
            }
            if (action.state == ImapActionState::kCompleted || action.state == ImapActionState::kCancelled) {
                continue;
            }
            action.state = ImapActionState::kRunning;
            action.updated_at = NowUnixSeconds();
            ++action.attempts;
            imap_action_store_->SaveAction(action, nullptr);

            std::string action_error;
            std::optional<AuthFailureDetails> action_auth_failure;
            TaskScope task(task_model_,
                           action.kind == ImapActionKind::kFetchAttachment
                                   ? MailTaskKind::kAttachmentFetch
                                   : MailTaskKind::kImapMutation,
                           action.account_id,
                           "Replay IMAP action",
                           "Running");
            if (ReplaySingleImapAction(action,
                                       account_service_,
                                       credential_store_,
                                       sync_state_store_,
                                       mailbox_store_,
                                       message_store_,
                                       transport_service_,
                                       tls_provider_,
                                       *gssapi_engine_,
                                       &action_auth_failure,
                                       &action_error)) {
                task.Complete("Complete");
                imap_action_store_->DeleteAction(action.id, nullptr);
            } else {
                if (action_auth_failure) {
                    task.Fail("Failed",
                              action_error,
                              action_auth_failure->kind,
                              action_auth_failure->mechanism);
                } else {
                    task.Fail("Failed", action_error);
                }
                action.state = stop_requested_ ? ImapActionState::kPending : ImapActionState::kFailed;
                action.updated_at = NowUnixSeconds();
                action.last_error = action_error;
                imap_action_store_->SaveAction(action, nullptr);
                if (!action_error.empty()) {
                    summary.warnings.push_back(action_error);
                }
            }
        }
    }

    for (const auto& account : accounts) {
        if (stop_requested_) {
            summary.warnings.push_back("Mail check stopped by user request.");
            break;
        }
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
            PopSession session(account, *password, message_store_, transport_service_, tls_provider_, *gssapi_engine_);
            if (!session.FetchMessages(&state, &messages, &error_message)) {
                if (session.LastAuthFailure()) {
                    task.Fail("Failed",
                              error_message,
                              session.LastAuthFailure()->kind,
                              session.LastAuthFailure()->mechanism);
                } else {
                    task.Fail("Failed", error_message);
                }
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
            ImapSession session(account, *password, message_store_, transport_service_, tls_provider_, *gssapi_engine_);
            std::vector<std::string> remote_mailboxes;
            if (!session.DiscoverMailboxes(&remote_mailboxes, &error_message)) {
                if (session.LastAuthFailure()) {
                    discovery_task.Fail("Failed",
                                        error_message,
                                        session.LastAuthFailure()->kind,
                                        session.LastAuthFailure()->mechanism);
                } else {
                    discovery_task.Fail("Failed", error_message);
                }
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
                ImapSession sync_session(
                    account, *password, message_store_, transport_service_, tls_provider_, *gssapi_engine_);
                if (!sync_session.SyncMailbox(remote_mailbox, &state, &messages, &error_message)) {
                    if (sync_session.LastAuthFailure()) {
                        sync_task.Fail("Failed",
                                       error_message,
                                       sync_session.LastAuthFailure()->kind,
                                       sync_session.LastAuthFailure()->mechanism);
                    } else {
                        sync_task.Fail("Failed", error_message);
                    }
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

MailTransportSummary MailTransportCoordinator::RefreshMailbox(std::string_view mailbox_id, bool full_resync) {
    MailTransportSummary summary;
    stop_requested_ = false;
    const auto mailbox = mailbox_store_.GetMailbox(mailbox_id);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap) {
        summary.error_message = "Mailbox is not an IMAP mailbox.";
        return summary;
    }

    const auto account = account_service_.FindById(mailbox->account_id);
    if (!account) {
        summary.error_message = "Account for mailbox is unavailable.";
        return summary;
    }

    const auto password = credential_store_.LoadCredential(account->id, CredentialKind::kIncoming);
    if (!password) {
        summary.error_message = "Missing incoming credential for account " + account->id;
        return summary;
    }

    if (imap_action_store_) {
        for (auto action : imap_action_store_->ListActions()) {
            if (action.account_id != account->id ||
                action.state == ImapActionState::kCompleted ||
                action.state == ImapActionState::kCancelled) {
                continue;
            }

            action.state = ImapActionState::kRunning;
            action.updated_at = NowUnixSeconds();
            ++action.attempts;
            imap_action_store_->SaveAction(action, nullptr);

            std::string action_error;
            std::optional<AuthFailureDetails> action_auth_failure;
            TaskScope task(task_model_,
                           action.kind == ImapActionKind::kFetchAttachment
                                   ? MailTaskKind::kAttachmentFetch
                                   : MailTaskKind::kImapMutation,
                           action.account_id,
                           "Replay IMAP action",
                           "Running");
            if (ReplaySingleImapAction(action,
                                       account_service_,
                                       credential_store_,
                                       sync_state_store_,
                                       mailbox_store_,
                                       message_store_,
                                       transport_service_,
                                       tls_provider_,
                                       *gssapi_engine_,
                                       &action_auth_failure,
                                       &action_error)) {
                task.Complete("Complete");
                imap_action_store_->DeleteAction(action.id, nullptr);
            } else {
                if (action_auth_failure) {
                    task.Fail("Failed",
                              action_error,
                              action_auth_failure->kind,
                              action_auth_failure->mechanism);
                } else {
                    task.Fail("Failed", action_error);
                }
                action.state = ImapActionState::kFailed;
                action.updated_at = NowUnixSeconds();
                action.last_error = action_error;
                imap_action_store_->SaveAction(action, nullptr);
                if (!action_error.empty()) {
                    summary.warnings.push_back(action_error);
                }
            }
        }
    }

    ImapMailboxSyncState state =
        sync_state_store_.LoadImapState(account->id, mailbox->id)
            .value_or(ImapMailboxSyncState{account->id, mailbox->id, 0, 0});
    if (full_resync) {
        state.last_seen_uid = 0;
    }

    std::vector<MessageRecord> messages;
    std::string error_message;
    ImapSession session(*account, *password, message_store_, transport_service_, tls_provider_, *gssapi_engine_);
    if (!session.SyncMailbox(mailbox->remote_name, &state, &messages, &error_message)) {
        summary.error_message = error_message;
        return summary;
    }
    sync_state_store_.SaveImapState(state, nullptr);
    for (const auto& message : messages) {
        message_store_.SaveMessage(message, nullptr);
        ++summary.messages_received;
    }
    summary.success = true;
    return summary;
}

bool MailTransportCoordinator::QueueDeleteMessage(std::string_view mailbox_id,
                                                  std::string_view message_id,
                                                  std::string* error_message) {
    if (!imap_action_store_) {
        if (error_message) {
            *error_message = "IMAP action store is unavailable.";
        }
        return false;
    }
    const auto mailbox = mailbox_store_.GetMailbox(mailbox_id);
    const auto message = message_store_.GetMessage(mailbox_id, message_id);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap || !message) {
        if (error_message) {
            *error_message = "IMAP message not found.";
        }
        return false;
    }
    const auto account = account_service_.FindById(mailbox->account_id);
    if (!account) {
        if (error_message) {
            *error_message = "Account for IMAP message is unavailable.";
        }
        return false;
    }

    ImapActionRecord action;
    action.id = GenerateId("imap-action");
    action.kind = ImapActionKind::kDelete;
    action.state = ImapActionState::kPending;
    action.account_id = account->id;
    action.mailbox_id = std::string(mailbox_id);
    action.remote_mailbox = mailbox->remote_name;
    action.message_id = std::string(message_id);
    action.remote_message_id = message->remote_id;
    action.created_at = NowUnixSeconds();
    action.updated_at = action.created_at;

    if (account->transfer_to_trash_on_delete) {
        action.destination_remote_mailbox =
            account->trash_mailbox_name.empty() ? "Trash" : account->trash_mailbox_name;
        if (!OptimisticallyMoveMessage(message_store_,
                                       mailbox_store_,
                                       *account,
                                       mailbox_id,
                                       message_id,
                                       action.destination_remote_mailbox,
                                       error_message)) {
            return false;
        }
    } else if (account->mark_as_deleted) {
        if (!SaveUpdatedMessage(message_store_,
                                mailbox_id,
                                message_id,
                                [](MessageRecord& updated) { updated.deleted = true; },
                                error_message)) {
            return false;
        }
    } else if (!message_store_.DeleteMessage(mailbox_id, message_id, error_message)) {
        return false;
    }

    return imap_action_store_->SaveAction(action, error_message);
}

bool MailTransportCoordinator::QueueUndeleteMessage(std::string_view mailbox_id,
                                                    std::string_view message_id,
                                                    std::string* error_message) {
    if (!imap_action_store_) {
        if (error_message) {
            *error_message = "IMAP action store is unavailable.";
        }
        return false;
    }
    const auto mailbox = mailbox_store_.GetMailbox(mailbox_id);
    const auto message = message_store_.GetMessage(mailbox_id, message_id);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap || !message) {
        if (error_message) {
            *error_message = "IMAP message not found.";
        }
        return false;
    }

    if (!SaveUpdatedMessage(message_store_,
                            mailbox_id,
                            message_id,
                            [](MessageRecord& updated) { updated.deleted = false; },
                            error_message)) {
        return false;
    }

    ImapActionRecord action;
    action.id = GenerateId("imap-action");
    action.kind = ImapActionKind::kUndelete;
    action.state = ImapActionState::kPending;
    action.account_id = mailbox->account_id;
    action.mailbox_id = std::string(mailbox_id);
    action.remote_mailbox = mailbox->remote_name;
    action.message_id = std::string(message_id);
    action.remote_message_id = message->remote_id;
    action.created_at = NowUnixSeconds();
    action.updated_at = action.created_at;
    return imap_action_store_->SaveAction(action, error_message);
}

bool MailTransportCoordinator::QueueMoveMessage(std::string_view mailbox_id,
                                                std::string_view message_id,
                                                std::string_view destination_mailbox_id,
                                                std::string* error_message) {
    if (!imap_action_store_) {
        if (error_message) {
            *error_message = "IMAP action store is unavailable.";
        }
        return false;
    }
    const auto source_mailbox = mailbox_store_.GetMailbox(mailbox_id);
    const auto destination_mailbox = mailbox_store_.GetMailbox(destination_mailbox_id);
    const auto message = message_store_.GetMessage(mailbox_id, message_id);
    if (!source_mailbox || !destination_mailbox || !message ||
        source_mailbox->protocol != MailboxProtocol::kImap ||
        destination_mailbox->protocol != MailboxProtocol::kImap) {
        if (error_message) {
            *error_message = "IMAP move source or destination is unavailable.";
        }
        return false;
    }
    const auto account = account_service_.FindById(source_mailbox->account_id);
    if (!account) {
        if (error_message) {
            *error_message = "Account for IMAP move is unavailable.";
        }
        return false;
    }
    if (!OptimisticallyMoveMessage(message_store_,
                                   mailbox_store_,
                                   *account,
                                   mailbox_id,
                                   message_id,
                                   destination_mailbox->remote_name,
                                   error_message)) {
        return false;
    }

    ImapActionRecord action;
    action.id = GenerateId("imap-action");
    action.kind = ImapActionKind::kMove;
    action.state = ImapActionState::kPending;
    action.account_id = account->id;
    action.mailbox_id = std::string(mailbox_id);
    action.remote_mailbox = source_mailbox->remote_name;
    action.message_id = std::string(message_id);
    action.remote_message_id = message->remote_id;
    action.destination_mailbox_id = std::string(destination_mailbox_id);
    action.destination_remote_mailbox = destination_mailbox->remote_name;
    action.created_at = NowUnixSeconds();
    action.updated_at = action.created_at;
    return imap_action_store_->SaveAction(action, error_message);
}

bool MailTransportCoordinator::QueueCopyMessage(std::string_view mailbox_id,
                                                std::string_view message_id,
                                                std::string_view destination_mailbox_id,
                                                std::string* error_message) {
    if (!imap_action_store_) {
        if (error_message) {
            *error_message = "IMAP action store is unavailable.";
        }
        return false;
    }
    const auto source_mailbox = mailbox_store_.GetMailbox(mailbox_id);
    const auto destination_mailbox = mailbox_store_.GetMailbox(destination_mailbox_id);
    const auto message = message_store_.GetMessage(mailbox_id, message_id);
    if (!source_mailbox || !destination_mailbox || !message ||
        source_mailbox->protocol != MailboxProtocol::kImap ||
        destination_mailbox->protocol != MailboxProtocol::kImap) {
        if (error_message) {
            *error_message = "IMAP copy source or destination is unavailable.";
        }
        return false;
    }
    const auto account = account_service_.FindById(source_mailbox->account_id);
    if (!account) {
        if (error_message) {
            *error_message = "Account for IMAP copy is unavailable.";
        }
        return false;
    }
    if (!OptimisticallyCopyMessage(message_store_,
                                   mailbox_store_,
                                   *account,
                                   mailbox_id,
                                   message_id,
                                   destination_mailbox->remote_name,
                                   error_message)) {
        return false;
    }

    ImapActionRecord action;
    action.id = GenerateId("imap-action");
    action.kind = ImapActionKind::kCopy;
    action.state = ImapActionState::kPending;
    action.account_id = account->id;
    action.mailbox_id = std::string(mailbox_id);
    action.remote_mailbox = source_mailbox->remote_name;
    action.message_id = std::string(message_id);
    action.remote_message_id = message->remote_id;
    action.destination_mailbox_id = std::string(destination_mailbox_id);
    action.destination_remote_mailbox = destination_mailbox->remote_name;
    action.created_at = NowUnixSeconds();
    action.updated_at = action.created_at;
    return imap_action_store_->SaveAction(action, error_message);
}

bool MailTransportCoordinator::QueueCreateMailbox(std::string_view account_id,
                                                  std::string_view remote_name,
                                                  std::string* error_message) {
    if (!imap_action_store_) {
        if (error_message) {
            *error_message = "IMAP action store is unavailable.";
        }
        return false;
    }
    const auto account = account_service_.FindById(account_id);
    if (!account) {
        if (error_message) {
            *error_message = "IMAP account is unavailable.";
        }
        return false;
    }
    if (!EnsureRemoteMailbox(mailbox_store_, *account, remote_name, error_message)) {
        return false;
    }
    ImapActionRecord action;
    action.id = GenerateId("imap-action");
    action.kind = ImapActionKind::kCreateMailbox;
    action.state = ImapActionState::kPending;
    action.account_id = account->id;
    action.mailbox_id = LocalMailboxIdForImap(account->id, remote_name);
    action.remote_mailbox = std::string(remote_name);
    action.created_at = NowUnixSeconds();
    action.updated_at = action.created_at;
    return imap_action_store_->SaveAction(action, error_message);
}

bool MailTransportCoordinator::QueueRenameMailbox(std::string_view mailbox_id,
                                                  std::string_view new_remote_name,
                                                  std::string* error_message) {
    if (!imap_action_store_) {
        if (error_message) {
            *error_message = "IMAP action store is unavailable.";
        }
        return false;
    }
    const auto mailbox = mailbox_store_.GetMailbox(mailbox_id);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap) {
        if (error_message) {
            *error_message = "IMAP mailbox is unavailable.";
        }
        return false;
    }
    const auto account = account_service_.FindById(mailbox->account_id);
    if (!account) {
        if (error_message) {
            *error_message = "IMAP account is unavailable.";
        }
        return false;
    }
    const std::string renamed_mailbox_id = LocalMailboxIdForImap(account->id, new_remote_name);
    if (!mailbox_store_.RenameMailbox(mailbox_id, renamed_mailbox_id, new_remote_name, error_message)) {
        return false;
    }
    if (!RenameMailboxMessages(message_store_,
                               mailbox_id,
                               renamed_mailbox_id,
                               new_remote_name,
                               error_message)) {
        return false;
    }
    ImapActionRecord action;
    action.id = GenerateId("imap-action");
    action.kind = ImapActionKind::kRenameMailbox;
    action.state = ImapActionState::kPending;
    action.account_id = account->id;
    action.mailbox_id = std::string(mailbox_id);
    action.remote_mailbox = mailbox->remote_name;
    action.rename_target = std::string(new_remote_name);
    action.created_at = NowUnixSeconds();
    action.updated_at = action.created_at;
    return imap_action_store_->SaveAction(action, error_message);
}

bool MailTransportCoordinator::QueueDeleteMailbox(std::string_view mailbox_id, std::string* error_message) {
    if (!imap_action_store_) {
        if (error_message) {
            *error_message = "IMAP action store is unavailable.";
        }
        return false;
    }
    const auto mailbox = mailbox_store_.GetMailbox(mailbox_id);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap) {
        if (error_message) {
            *error_message = "IMAP mailbox is unavailable.";
        }
        return false;
    }
    if (!DeleteMailboxMessages(message_store_, mailbox_id, error_message)) {
        return false;
    }
    if (!mailbox_store_.DeleteMailbox(mailbox_id, error_message)) {
        return false;
    }
    ImapActionRecord action;
    action.id = GenerateId("imap-action");
    action.kind = ImapActionKind::kDeleteMailbox;
    action.state = ImapActionState::kPending;
    action.account_id = mailbox->account_id;
    action.mailbox_id = std::string(mailbox_id);
    action.remote_mailbox = mailbox->remote_name;
    action.created_at = NowUnixSeconds();
    action.updated_at = action.created_at;
    return imap_action_store_->SaveAction(action, error_message);
}

bool MailTransportCoordinator::QueueFetchAttachment(std::string_view mailbox_id,
                                                    std::string_view message_id,
                                                    std::size_t attachment_index,
                                                    std::string* error_message) {
    if (!imap_action_store_) {
        if (error_message) {
            *error_message = "IMAP action store is unavailable.";
        }
        return false;
    }
    const auto mailbox = mailbox_store_.GetMailbox(mailbox_id);
    const auto message = message_store_.GetMessage(mailbox_id, message_id);
    if (!mailbox || !message || mailbox->protocol != MailboxProtocol::kImap) {
        if (error_message) {
            *error_message = "IMAP message is unavailable.";
        }
        return false;
    }
    if (attachment_index >= message->attachments.size()) {
        if (error_message) {
            *error_message = "Attachment index is out of range.";
        }
        return false;
    }
    ImapActionRecord action;
    action.id = GenerateId("imap-action");
    action.kind = ImapActionKind::kFetchAttachment;
    action.state = ImapActionState::kPending;
    action.account_id = mailbox->account_id;
    action.mailbox_id = std::string(mailbox_id);
    action.remote_mailbox = mailbox->remote_name;
    action.message_id = std::string(message_id);
    action.remote_message_id = message->remote_id;
    action.attachment_index = attachment_index;
    action.created_at = NowUnixSeconds();
    action.updated_at = action.created_at;
    return imap_action_store_->SaveAction(action, error_message);
}

bool MailTransportCoordinator::QueueFetchFullMessage(std::string_view mailbox_id,
                                                     std::string_view message_id,
                                                     std::string* error_message) {
    if (!imap_action_store_) {
        if (error_message) {
            *error_message = "IMAP action store is unavailable.";
        }
        return false;
    }
    const auto mailbox = mailbox_store_.GetMailbox(mailbox_id);
    const auto message = message_store_.GetMessage(mailbox_id, message_id);
    if (!mailbox || !message || mailbox->protocol != MailboxProtocol::kImap) {
        if (error_message) {
            *error_message = "IMAP message is unavailable.";
        }
        return false;
    }
    ImapActionRecord action;
    action.id = GenerateId("imap-action");
    action.kind = ImapActionKind::kFetchFullMessage;
    action.state = ImapActionState::kPending;
    action.account_id = mailbox->account_id;
    action.mailbox_id = std::string(mailbox_id);
    action.remote_mailbox = mailbox->remote_name;
    action.message_id = std::string(message_id);
    action.remote_message_id = message->remote_id;
    action.created_at = NowUnixSeconds();
    action.updated_at = action.created_at;
    return imap_action_store_->SaveAction(action, error_message);
}

bool MailTransportCoordinator::RetryImapAction(std::string_view action_id, std::string* error_message) {
    if (!imap_action_store_) {
        if (error_message) {
            *error_message = "IMAP action store is unavailable.";
        }
        return false;
    }
    const auto action = imap_action_store_->GetAction(action_id);
    if (!action) {
        if (error_message) {
            *error_message = "IMAP action not found.";
        }
        return false;
    }
    ImapActionRecord updated = *action;
    updated.state = ImapActionState::kPending;
    updated.updated_at = NowUnixSeconds();
    updated.last_error.clear();
    return imap_action_store_->SaveAction(updated, error_message);
}

bool MailTransportCoordinator::CancelImapAction(std::string_view action_id, std::string* error_message) {
    if (!imap_action_store_) {
        if (error_message) {
            *error_message = "IMAP action store is unavailable.";
        }
        return false;
    }
    const auto action = imap_action_store_->GetAction(action_id);
    if (!action) {
        if (error_message) {
            *error_message = "IMAP action not found.";
        }
        return false;
    }
    ImapActionRecord updated = *action;
    updated.state = ImapActionState::kCancelled;
    updated.updated_at = NowUnixSeconds();
    return imap_action_store_->SaveAction(updated, error_message);
}

bool MailTransportCoordinator::StopActiveTasks() {
    stop_requested_ = true;
    return true;
}

}  // namespace hermes
