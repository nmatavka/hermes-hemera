#include "EudoraStorage.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <ctime>
#include <utility>
#include <vector>

#include "hermes/IniSettingsStore.h"
#include "hermes/LegacyMessageStatus.h"
#include "hermes/PopServerStatus.h"
#include "hermes/RichTextFormat.h"

namespace hermes::eudora {

namespace {

constexpr std::uint16_t kTocVersion = 0x31;
constexpr std::size_t kUsedVersionBytes = 6;
constexpr std::size_t kTocNameBytes = 32;
constexpr std::size_t kSavedColumnWidths = 8;
constexpr std::size_t kSavedSortColumns = 9;
constexpr std::size_t kUnusedTocBytes = 2;
constexpr std::size_t kUnusedSummaryBytes = 2;

constexpr std::uint16_t kMsfWordWrap = 0x0004;
constexpr std::uint16_t kMsfTabsInBody = 0x0008;
constexpr std::uint16_t kMsfKeepCopies = 0x0010;
constexpr std::uint16_t kMsfTextAsDoc = 0x0020;
constexpr std::uint16_t kMsfReturnReceipt = 0x0040;
constexpr std::uint16_t kMsfQuotedPrintable = 0x0080;
constexpr std::uint16_t kMsfMime = 0x0100;
constexpr std::uint16_t kMsfUuencode = 0x0200;
constexpr std::uint16_t kMsfXrich = 0x2000;
constexpr std::uint16_t kMsfReadReceipt = 0x4000;
constexpr std::uint16_t kMsfHasAttachment = 0x8000;

constexpr std::uint16_t kMsfExHtml = 0x0002;
constexpr std::uint16_t kMsfExMdn = 0x0004;
constexpr std::uint16_t kMsfExSendPlain = 0x0010;
constexpr std::uint16_t kMsfExSendStyled = 0x0020;
constexpr std::uint16_t kMsfExFlowed = 0x0040;
constexpr std::uint16_t kMsfExEmptyBody = 0x0100;

constexpr std::uint32_t kImapSeen = 0x00000001;
constexpr std::uint32_t kImapAnswered = 0x00000002;
constexpr std::uint32_t kImapFlagged = 0x00000004;
constexpr std::uint32_t kImapDeleted = 0x00000008;
constexpr std::uint32_t kImapDraft = 0x00000010;
constexpr std::uint32_t kImapRecent = 0x00000020;
constexpr std::uint32_t kImapUndownloadedAttachments = 0x001F0000;
constexpr int kImapUndownloadedShift = 16;
constexpr std::uint32_t kImapNotDownloaded = 0x40000000;
constexpr std::uint32_t kImapFullHeader = 0x80000000;

constexpr char kStateUnread = 0;
constexpr char kStateRead = 1;
constexpr char kStateReplied = 2;
constexpr char kStateForwarded = 3;
constexpr char kStateRedirected = 4;
constexpr char kStateUnsendable = 5;
constexpr char kStateSendable = 6;
constexpr char kStateQueued = 7;
constexpr char kStateSent = 8;
constexpr char kStateUnsent = 9;
constexpr char kStateTimeQueued = 10;
constexpr char kStateSpooled = 11;
constexpr char kStateRecovered = 12;

constexpr std::array<short, kSavedColumnWidths> kDefaultColumnWidths = {1, 4, 1, 1, 8, 16, 16, 2};
constexpr std::array<std::uint8_t, kSavedSortColumns> kDefaultSortMethods = {0, 0, 0, 0, 0, 0, 0, 0, 0};

constexpr const char* kDescmapName = "descmap.pce";
constexpr const char* kImapMboxListName = "mboxlist.lst";
constexpr const char* kAttachDirectoryName = "Attach";
constexpr const char* kPendingAttachmentDirectoryName = "Pending";
constexpr const char* kImapDirectoryName = "IMAP";
constexpr const char* kDraftSidecarDirectoryName = "DraftState";
constexpr const char* kLegacyMailboxesDirectoryName = "mailboxes";
constexpr const char* kLegacyDraftsDirectoryName = "drafts";
constexpr const char* kLegacyAttachmentsDirectoryName = "Attachments";
constexpr const char* kEnvelopePrefix = "From ???@??? ";
constexpr const char* kMimeBoundaryMixed = "hemera-mixed";
constexpr const char* kMimeBoundaryAlternative = "hemera-alternative";

thread_local bool g_initializing_canonical_store = false;

enum class UnreadStatus : short {
    kUnknown = 0,
    kYes = 1,
    kNo = 2,
};

enum class LegacyMailboxType : short {
    kIn = 0,
    kOut = 1,
    kJunk = 2,
    kTrash = 3,
    kRegular = 4,
    kFolder = 5,
    kImapAccount = 6,
    kImapNamespace = 7,
    kImapMailbox = 8,
};

struct TocSummary {
    std::int32_t offset = 0;
    std::int32_t length = 0;
    std::int32_t seconds = 0;
    std::int32_t arrival_seconds = 0;
    std::int32_t hash = 0;
    std::int32_t unique_id = 0;
    std::int32_t persona_hash = 0;
    std::int32_t junk_plugin_id = 0;
    std::int16_t saved_left = 0;
    std::int16_t saved_top = 0;
    std::int16_t saved_right = 400;
    std::int16_t saved_bottom = 300;
    std::int16_t label = 0;
    std::int16_t timezone_minutes = 0;
    std::uint16_t flags = 0;
    std::uint16_t flags_ex = 0;
    std::uint16_t message_size_kb = 1;
    std::uint32_t imap_flags = 0;
    char state = kStateRead;
    char priority = 3;
    char mood = 0;
    std::uint8_t junk_score = 0;
    bool manually_junked = false;
    std::array<char, 32> date{};
    std::array<char, 64> from{};
    std::array<char, 64> subject{};
};

struct TocData {
    std::uint16_t version = kTocVersion;
    std::array<std::uint8_t, kUsedVersionBytes> used_version{};
    std::string name;
    LegacyMailboxType type = LegacyMailboxType::kRegular;
    bool group_by_subject = false;
    bool needs_sorting = false;
    bool show_file_browser = false;
    int file_browser_view_state = 0;
    bool hide_deleted_imap = false;
    bool needs_compact = false;
    std::int16_t saved_left = 0;
    std::int16_t saved_top = 0;
    std::int16_t saved_right = 800;
    std::int16_t saved_bottom = 600;
    std::array<std::int16_t, kSavedColumnWidths> field_widths = kDefaultColumnWidths;
    UnreadStatus unread_status = UnreadStatus::kUnknown;
    std::int32_t next_unique_message_id = 0;
    std::int32_t plugin_id = 0;
    std::int32_t plugin_tag = 0;
    std::int16_t splitter_pos = 0;
    std::array<std::uint8_t, kSavedSortColumns> sort_methods = kDefaultSortMethods;
    std::uint8_t ad_failure = 0;
    std::uint32_t stored_mbx_size_plus_one = 0;
    std::vector<TocSummary> summaries;
};

struct ResolvedMailbox {
    MailboxRecord record;
    std::filesystem::path scope_directory;
    std::filesystem::path mbx_path;
    std::filesystem::path toc_path;
    std::filesystem::path imap_account_root;
    std::string imap_directory_name;
};

struct ParsedStoredMessage {
    MessageRecord record;
    std::string logical_id;
    std::string draft_id;
    std::string reply_to;
    bool has_explicit_legacy_status = false;
    bool has_explicit_label = false;
    bool has_explicit_junk_score = false;
    bool has_explicit_manually_junked = false;
    bool has_explicit_pop_server_status = false;
    std::vector<std::string> embedded_attachment_payloads;
};

struct StoredMessageEntry {
    TocSummary summary;
    ParsedStoredMessage parsed;
};

std::int64_t NowUnixSeconds() {
    return static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
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

std::string NormalizeHemeraHeaderLine(std::string line) {
    static constexpr std::string_view kLegacyPrefix = "X-Hermes-";
    static constexpr std::string_view kCurrentPrefix = "X-Hemera-";
    if (StartsWithInsensitive(line, kLegacyPrefix)) {
        return std::string(kCurrentPrefix) + line.substr(kLegacyPrefix.size());
    }
    return line;
}

std::string NormalizeHemeraHeaderKey(std::string key) {
    static constexpr std::string_view kLegacyPrefix = "x-hermes-";
    static constexpr std::string_view kCurrentPrefix = "x-hemera-";
    if (StartsWithInsensitive(key, kLegacyPrefix)) {
        return std::string(kCurrentPrefix) + key.substr(kLegacyPrefix.size());
    }
    return key;
}

bool IsHemeraPaigeMimeType(std::string_view content_type) {
    return content_type.find("application/x-hemera-paige") != std::string::npos ||
           content_type.find("application/x-hermes-paige") != std::string::npos;
}

bool IsDigitsOnly(std::string_view value) {
    return !value.empty() &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; });
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

std::string TrimQuotes(std::string value) {
    value = Trim(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string SanitizePathComponent(std::string value) {
    if (value.empty()) {
        return "Mailbox";
    }
    for (char& ch : value) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (!(std::isalnum(uch) || ch == '.' || ch == '_' || ch == '-')) {
            ch = '-';
        }
    }
    while (!value.empty() && (value.front() == '.' || value.front() == '-')) {
        value.erase(value.begin());
    }
    if (value.empty()) {
        value = "Mailbox";
    }
    return value;
}

std::string SanitizeAttachmentName(std::string value) {
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

std::uint32_t Fnv1a(std::string_view value) {
    std::uint32_t hash = 2166136261u;
    for (unsigned char ch : value) {
        hash ^= ch;
        hash *= 16777619u;
    }
    return hash;
}

std::string FormatEnvelopeLine(std::int64_t timestamp) {
    std::time_t seconds = timestamp > 0 ? static_cast<std::time_t>(timestamp) : std::time(nullptr);
    std::tm local_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &seconds);
#else
    localtime_r(&seconds, &local_tm);
#endif
    char buffer[128];
    if (std::strftime(buffer, sizeof(buffer), "From ???@??? %a %b %d %H:%M:%S %Y", &local_tm) == 0) {
        return "From ???@??? Thu Jan 01 00:00:01 1970";
    }
    return buffer;
}

std::string FormatSummaryDate(std::int64_t timestamp) {
    std::time_t seconds = timestamp > 0 ? static_cast<std::time_t>(timestamp) : std::time(nullptr);
    std::tm local_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &seconds);
#else
    localtime_r(&seconds, &local_tm);
#endif
    char buffer[32];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local_tm) == 0) {
        return "1970-01-01 00:00";
    }
    return buffer;
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

std::string EncodeQuotedPrintable(std::string_view input) {
    auto hex = [](unsigned char value) {
        static constexpr char kHex[] = "0123456789ABCDEF";
        std::string out;
        out.push_back('=');
        out.push_back(kHex[(value >> 4) & 0x0F]);
        out.push_back(kHex[value & 0x0F]);
        return out;
    };

    std::string encoded;
    std::size_t line_length = 0;
    auto append_chunk = [&](std::string_view chunk) {
        if (line_length + chunk.size() > 73) {
            encoded += "=\r\n";
            line_length = 0;
        }
        encoded.append(chunk);
        line_length += chunk.size();
    };

    for (std::size_t index = 0; index < input.size(); ++index) {
        const unsigned char ch = static_cast<unsigned char>(input[index]);
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            encoded += "\r\n";
            line_length = 0;
            continue;
        }
        const bool printable = (ch >= 33 && ch <= 60) || (ch >= 62 && ch <= 126);
        const bool trailing_space =
            (ch == ' ' || ch == '\t') &&
            (index + 1 == input.size() || input[index + 1] == '\n' || input[index + 1] == '\r');
        if (printable && !trailing_space) {
            append_chunk(std::string_view(reinterpret_cast<const char*>(&ch), 1));
            continue;
        }
        const std::string escaped = hex(ch);
        append_chunk(escaped);
    }

    return encoded;
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

bool ReadPrimitive(std::istream& input, void* buffer, std::size_t size) {
    input.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(input.gcount()) == size;
}

template <typename Integer>
bool ReadLittleEndian(std::istream& input, Integer* value) {
    std::array<unsigned char, sizeof(Integer)> bytes{};
    if (!ReadPrimitive(input, bytes.data(), bytes.size())) {
        return false;
    }
    std::uint64_t assembled = 0;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        assembled |= static_cast<std::uint64_t>(bytes[index]) << (index * 8);
    }
    *value = static_cast<Integer>(assembled);
    return true;
}

template <typename Integer>
void WriteLittleEndian(std::ostream& output, Integer value) {
    std::array<unsigned char, sizeof(Integer)> bytes{};
    std::uint64_t raw = static_cast<std::uint64_t>(value);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<unsigned char>((raw >> (index * 8)) & 0xFF);
    }
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

template <std::size_t Size>
void WriteFixedString(std::ostream& output, std::string_view value) {
    std::array<char, Size> buffer{};
    const std::size_t count = std::min(value.size(), Size ? Size - 1 : 0);
    if (count > 0) {
        std::copy_n(value.data(), count, buffer.data());
    }
    output.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
}

template <std::size_t Size>
bool ReadFixedString(std::istream& input, std::array<char, Size>* buffer) {
    return ReadPrimitive(input, buffer->data(), buffer->size());
}

std::string FixedStringView(const char* data, std::size_t size) {
    const auto* end = static_cast<const char*>(std::memchr(data, '\0', size));
    return std::string(data, end == nullptr ? size : static_cast<std::size_t>(end - data));
}

std::filesystem::path AttachRoot(const std::filesystem::path& root_directory) {
    return root_directory / kAttachDirectoryName;
}

std::filesystem::path PendingAttachmentRoot(const std::filesystem::path& root_directory) {
    return AttachRoot(root_directory) / kPendingAttachmentDirectoryName;
}

std::filesystem::path ImapRoot(const std::filesystem::path& root_directory) {
    return root_directory / kImapDirectoryName;
}

std::filesystem::path DraftSidecarRoot(const std::filesystem::path& root_directory) {
    return root_directory / kDraftSidecarDirectoryName;
}

std::filesystem::path LocalDescmapPath(const std::filesystem::path& scope_directory) {
    return scope_directory / kDescmapName;
}

std::filesystem::path ImapMboxListPath(const std::filesystem::path& account_root) {
    return account_root / kImapMboxListName;
}

std::filesystem::path LegacyMailboxesPath(const std::filesystem::path& root_directory) {
    return root_directory / kLegacyMailboxesDirectoryName;
}

std::filesystem::path LegacyDraftsPath(const std::filesystem::path& root_directory) {
    return root_directory / kLegacyDraftsDirectoryName;
}

std::filesystem::path LegacyAttachmentsPath(const std::filesystem::path& root_directory) {
    return root_directory / kLegacyAttachmentsDirectoryName;
}

std::filesystem::path MailboxMbxPathFromStem(const std::filesystem::path& scope_directory, std::string_view stem) {
    return scope_directory / (std::string(stem) + ".mbx");
}

std::filesystem::path MailboxTocPathFromStem(const std::filesystem::path& scope_directory, std::string_view stem) {
    return scope_directory / (std::string(stem) + ".toc");
}

std::optional<std::string> SystemMailboxStem(std::string_view mailbox_id) {
    const std::string lowered = ToLower(std::string(mailbox_id));
    if (lowered == "inbox") {
        return "In";
    }
    if (lowered == "out") {
        return "Out";
    }
    if (lowered == "trash") {
        return "Trash";
    }
    if (lowered == "junk") {
        return "Junk";
    }
    if (lowered == "sent") {
        return "Sent";
    }
    if (lowered == "drafts") {
        return "Drafts";
    }
    return std::nullopt;
}

std::optional<std::string> SystemMailboxIdFromStem(std::string_view stem) {
    const std::string lowered = ToLower(std::string(stem));
    if (lowered == "in") {
        return "inbox";
    }
    if (lowered == "out") {
        return "out";
    }
    if (lowered == "trash") {
        return "trash";
    }
    if (lowered == "junk") {
        return "junk";
    }
    if (lowered == "sent") {
        return "sent";
    }
    if (lowered == "drafts") {
        return "drafts";
    }
    return std::nullopt;
}

bool IsSystemMailboxId(std::string_view mailbox_id) {
    return SystemMailboxStem(mailbox_id).has_value();
}

std::string DisplayNameForMailbox(const MailboxRecord& mailbox) {
    if (!mailbox.display_name.empty()) {
        return mailbox.display_name;
    }
    if (const auto system = SystemMailboxStem(mailbox.id)) {
        return *system == "In"   ? "Inbox"
               : *system == "Out" ? "Out"
               : *system == "Trash" ? "Trash"
               : *system == "Junk"  ? "Junk"
               : *system == "Sent"  ? "Sent"
                                      : "Drafts";
    }
    return mailbox.id;
}

LegacyMailboxType LocalMailboxType(const MailboxRecord& mailbox) {
    const std::string lowered = ToLower(mailbox.id);
    if (lowered == "inbox") {
        return LegacyMailboxType::kIn;
    }
    if (lowered == "out") {
        return LegacyMailboxType::kOut;
    }
    if (lowered == "trash") {
        return LegacyMailboxType::kTrash;
    }
    if (lowered == "junk") {
        return LegacyMailboxType::kJunk;
    }
    if (mailbox.kind == MailboxKind::kFolder) {
        return LegacyMailboxType::kFolder;
    }
    return LegacyMailboxType::kRegular;
}

std::filesystem::path AttachmentStoragePath(const std::filesystem::path& root_directory,
                                            std::uint32_t unique_id,
                                            std::size_t attachment_index,
                                            std::string_view suggested_name) {
    return AttachRoot(root_directory) /
           (std::to_string(unique_id) + "-" + std::to_string(attachment_index) + "-" +
            SanitizeAttachmentName(std::string(suggested_name.empty() ? "attachment.bin" : suggested_name)));
}

std::filesystem::path PendingAttachmentStoragePath(const std::filesystem::path& root_directory,
                                                   std::string_view mailbox_id,
                                                   std::string_view message_id,
                                                   std::size_t attachment_index,
                                                   std::string_view suggested_name) {
    return PendingAttachmentRoot(root_directory) / SanitizePathComponent(std::string(mailbox_id)) /
           SanitizePathComponent(std::string(message_id)) /
           (std::to_string(attachment_index) + "-" +
            SanitizeAttachmentName(std::string(suggested_name.empty() ? "attachment.bin" : suggested_name)));
}

std::optional<std::filesystem::path> ExistingAttachmentPath(const std::filesystem::path& root_directory,
                                                            std::uint32_t unique_id,
                                                            std::size_t attachment_index) {
    const std::filesystem::path directory = AttachRoot(root_directory);
    if (!std::filesystem::exists(directory)) {
        return std::nullopt;
    }
    const std::string prefix = std::to_string(unique_id) + "-" + std::to_string(attachment_index) + "-";
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

std::optional<std::filesystem::path> ExistingPendingAttachmentPath(const std::filesystem::path& root_directory,
                                                                   std::string_view mailbox_id,
                                                                   std::string_view message_id,
                                                                   std::size_t attachment_index) {
    const std::filesystem::path directory =
        PendingAttachmentRoot(root_directory) / SanitizePathComponent(std::string(mailbox_id)) /
        SanitizePathComponent(std::string(message_id));
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
    value = ToLower(std::move(value));
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

LegacyMessageStatus LegacyStatusFromSummaryState(char state) {
    switch (state) {
        case kStateUnread:
            return LegacyMessageStatus::kUnread;
        case kStateRead:
            return LegacyMessageStatus::kRead;
        case kStateReplied:
            return LegacyMessageStatus::kReplied;
        case kStateForwarded:
            return LegacyMessageStatus::kForwarded;
        case kStateRedirected:
            return LegacyMessageStatus::kRedirected;
        case kStateUnsendable:
            return LegacyMessageStatus::kUnsendable;
        case kStateSendable:
            return LegacyMessageStatus::kSendable;
        case kStateQueued:
            return LegacyMessageStatus::kQueued;
        case kStateSent:
            return LegacyMessageStatus::kSent;
        case kStateUnsent:
            return LegacyMessageStatus::kUnsent;
        case kStateTimeQueued:
            return LegacyMessageStatus::kTimeQueued;
        case kStateSpooled:
            return LegacyMessageStatus::kSpooled;
        case kStateRecovered:
            return LegacyMessageStatus::kRecovered;
        default:
            return LegacyMessageStatus::kRead;
    }
}

LegacyMessageStatus DefaultLegacyStatusForMessage(const MessageRecord& message) {
    if (message.unread) {
        return LegacyMessageStatus::kUnread;
    }
    if (message.delivery_state == MessageDeliveryState::kQueued) {
        return LegacyMessageStatus::kQueued;
    }
    if (message.delivery_state == MessageDeliveryState::kSent) {
        return LegacyMessageStatus::kSent;
    }
    if (message.delivery_state == MessageDeliveryState::kDraft) {
        return LegacyMessageStatus::kUnsent;
    }
    if (message.answered) {
        return LegacyMessageStatus::kReplied;
    }
    return LegacyMessageStatus::kRead;
}

char SummaryStateForLegacyStatus(LegacyMessageStatus status) {
    switch (status) {
        case LegacyMessageStatus::kUnread:
            return kStateUnread;
        case LegacyMessageStatus::kRead:
            return kStateRead;
        case LegacyMessageStatus::kReplied:
            return kStateReplied;
        case LegacyMessageStatus::kForwarded:
            return kStateForwarded;
        case LegacyMessageStatus::kRedirected:
            return kStateRedirected;
        case LegacyMessageStatus::kUnsendable:
            return kStateUnsendable;
        case LegacyMessageStatus::kSendable:
            return kStateSendable;
        case LegacyMessageStatus::kQueued:
            return kStateQueued;
        case LegacyMessageStatus::kSent:
            return kStateSent;
        case LegacyMessageStatus::kUnsent:
            return kStateUnsent;
        case LegacyMessageStatus::kTimeQueued:
            return kStateTimeQueued;
        case LegacyMessageStatus::kSpooled:
            return kStateSpooled;
        case LegacyMessageStatus::kRecovered:
            return kStateRecovered;
    }
    return kStateRead;
}

std::string PriorityToString(ComposePriority priority) {
    switch (priority) {
        case ComposePriority::kHighest:
            return "highest";
        case ComposePriority::kHigh:
            return "high";
        case ComposePriority::kNormal:
            return "normal";
        case ComposePriority::kLow:
            return "low";
        case ComposePriority::kLowest:
            return "lowest";
    }
    return "normal";
}

ComposePriority PriorityFromString(const std::string& value) {
    if (value == "highest") {
        return ComposePriority::kHighest;
    }
    if (value == "high") {
        return ComposePriority::kHigh;
    }
    if (value == "low") {
        return ComposePriority::kLow;
    }
    if (value == "lowest") {
        return ComposePriority::kLowest;
    }
    return ComposePriority::kNormal;
}

char LegacyPriority(ComposePriority priority) {
    switch (priority) {
        case ComposePriority::kHighest:
            return 1;
        case ComposePriority::kHigh:
            return 2;
        case ComposePriority::kNormal:
            return 3;
        case ComposePriority::kLow:
            return 4;
        case ComposePriority::kLowest:
            return 5;
    }
    return 3;
}

ComposePriority ComposePriorityFromLegacy(char priority) {
    switch (priority) {
        case 1:
            return ComposePriority::kHighest;
        case 2:
            return ComposePriority::kHigh;
        case 4:
            return ComposePriority::kLow;
        case 5:
            return ComposePriority::kLowest;
        default:
            return ComposePriority::kNormal;
    }
}

std::string AttachmentEncodingToString(AttachmentEncodingMode mode) {
    switch (mode) {
        case AttachmentEncodingMode::kMime:
            return "mime";
        case AttachmentEncodingMode::kBinHex:
            return "binhex";
        case AttachmentEncodingMode::kUuencode:
            return "uuencode";
    }
    return "mime";
}

AttachmentEncodingMode AttachmentEncodingFromString(const std::string& value) {
    if (value == "binhex") {
        return AttachmentEncodingMode::kBinHex;
    }
    if (value == "uuencode") {
        return AttachmentEncodingMode::kUuencode;
    }
    return AttachmentEncodingMode::kMime;
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

std::string WrapPlainText(std::string_view text, std::size_t width) {
    if (width < 20) {
        return std::string(text);
    }

    std::string wrapped;
    std::size_t line_length = 0;
    std::size_t word_start = 0;
    auto flush_word = [&](std::size_t end) {
        const std::string_view word = text.substr(word_start, end - word_start);
        if (word.empty()) {
            return;
        }
        if (line_length != 0 && line_length + 1 + word.size() > width) {
            wrapped.push_back('\n');
            line_length = 0;
        } else if (line_length != 0) {
            wrapped.push_back(' ');
            ++line_length;
        }
        wrapped.append(word.data(), word.size());
        line_length += word.size();
    };

    for (std::size_t index = 0; index <= text.size(); ++index) {
        const bool at_end = index == text.size();
        const char ch = at_end ? '\0' : text[index];
        if (!at_end && ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            continue;
        }
        flush_word(index);
        word_start = index + 1;
        if (at_end) {
            break;
        }
        if (ch == '\n') {
            wrapped.push_back('\n');
            line_length = 0;
        } else if (ch == '\r') {
            if (index + 1 < text.size() && text[index + 1] == '\n') {
                continue;
            }
            wrapped.push_back('\n');
            line_length = 0;
        }
    }

    return wrapped;
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

std::string TrimTrailingLineEndings(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
        value.pop_back();
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

std::string BuildImapFlagsHeader(const MessageRecord& message) {
    std::string flags;
    if (!message.unread) {
        flags.push_back('R');
    }
    if (message.answered) {
        flags.push_back('A');
    }
    if (message.flagged) {
        flags.push_back('F');
    }
    if (message.deleted) {
        flags.push_back('D');
    }
    if (message.delivery_state == MessageDeliveryState::kDraft) {
        flags.push_back('T');
    }
    return flags;
}

std::string BuildStoredBodyPart(const MessageRecord& message,
                                const std::vector<std::string>& attachment_payloads) {
    std::ostringstream output;
    const bool quoted_printable = message.compose_options.quoted_printable;
    const std::string body_encoding = quoted_printable ? "quoted-printable" : "8bit";
    const bool has_styled_alternatives =
        !message.html_body.empty() || !message.rtf_body.empty() || !message.paige_native_body.empty();
    if (!message.attachments.empty()) {
        output << "Content-Type: multipart/mixed; boundary=\"" << kMimeBoundaryMixed << "\"\r\n\r\n";
        output << "--" << kMimeBoundaryMixed << "\r\n";
    }

    if (has_styled_alternatives) {
        output << "Content-Type: multipart/alternative; boundary=\"" << kMimeBoundaryAlternative << "\"\r\n\r\n";
        output << "--" << kMimeBoundaryAlternative << "\r\n";
        output << "Content-Type: text/plain; charset=UTF-8\r\n";
        output << "Content-Transfer-Encoding: " << body_encoding << "\r\n\r\n";
        output << (quoted_printable ? EncodeQuotedPrintable(message.plain_text_body) : message.plain_text_body)
               << "\r\n";
        if (!message.html_body.empty()) {
            output << "--" << kMimeBoundaryAlternative << "\r\n";
            output << "Content-Type: text/html; charset=UTF-8\r\n";
            output << "Content-Transfer-Encoding: " << body_encoding << "\r\n\r\n";
            output << (quoted_printable ? EncodeQuotedPrintable(message.html_body) : message.html_body) << "\r\n";
        }
        if (!message.rtf_body.empty()) {
            output << "--" << kMimeBoundaryAlternative << "\r\n";
            output << "Content-Type: application/rtf\r\n";
            output << "Content-Transfer-Encoding: " << body_encoding << "\r\n\r\n";
            output << (quoted_printable ? EncodeQuotedPrintable(message.rtf_body) : message.rtf_body) << "\r\n";
        }
        if (!message.paige_native_body.empty()) {
            output << "--" << kMimeBoundaryAlternative << "\r\n";
            output << "Content-Type: application/x-hemera-paige\r\n";
            output << "Content-Transfer-Encoding: base64\r\n\r\n";
            output << WrapBase64(Base64Encode(message.paige_native_body)) << "\r\n";
        }
        output << "--" << kMimeBoundaryAlternative << "--\r\n";
    } else {
        output << "Content-Type: text/plain; charset=UTF-8\r\n";
        output << "Content-Transfer-Encoding: " << body_encoding << "\r\n\r\n";
        output << (quoted_printable ? EncodeQuotedPrintable(message.plain_text_body) : message.plain_text_body)
               << "\r\n";
    }

    if (!message.attachments.empty()) {
        for (std::size_t index = 0; index < message.attachments.size(); ++index) {
            const auto& attachment = message.attachments[index];
            if (attachment.omitted || !attachment.download_complete) {
                continue;
            }
            const std::string payload = index < attachment_payloads.size() ? attachment_payloads[index] : std::string();
            output << "--" << kMimeBoundaryMixed << "\r\n";
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
        output << "--" << kMimeBoundaryMixed << "--\r\n";
    }
    return output.str();
}

std::string BuildStoredMessageText(const MessageRecord& message,
                                   std::uint32_t unique_id,
                                   std::string_view logical_id,
                                   const std::vector<std::string>& attachment_payloads) {
    std::ostringstream output;
    output << FormatEnvelopeLine(message.updated_at != 0 ? message.updated_at : message.created_at) << "\r\n";
    output << "X-Hemera-Unique-Id: " << unique_id << "\r\n";
    if (!logical_id.empty()) {
        output << "X-Hemera-Logical-Id: " << logical_id << "\r\n";
    }
    output << "X-Hemera-Account-Id: " << message.account_id << "\r\n";
    output << "X-Hemera-Delivery-State: " << DeliveryStateToString(message.delivery_state) << "\r\n";
    output << "X-Hemera-Remote-Id: " << message.remote_id << "\r\n";
    output << "X-Hemera-Remote-Mailbox: " << message.remote_mailbox << "\r\n";
    output << "X-Hemera-Download-Complete: " << (message.download_complete ? "1" : "0") << "\r\n";
    output << "X-Hemera-Attachments-Omitted: " << (message.attachments_omitted ? "1" : "0") << "\r\n";
    output << "X-Hemera-Flagged: " << (message.flagged ? "1" : "0") << "\r\n";
    output << "X-Hemera-Deleted: " << (message.deleted ? "1" : "0") << "\r\n";
    output << "X-Hemera-Answered: " << (message.answered ? "1" : "0") << "\r\n";
    output << "X-Hemera-Filters-Applied: " << (message.filters_applied ? "1" : "0") << "\r\n";
    output << "X-Hemera-Last-Error: " << message.last_error << "\r\n";
    output << "X-Hemera-Created-At: " << message.created_at << "\r\n";
    output << "X-Hemera-Updated-At: " << message.updated_at << "\r\n";
    output << "X-Hemera-Unread: " << (message.unread ? "1" : "0") << "\r\n";
    output << "X-Hemera-Legacy-Status: " << ToString(message.legacy_status) << "\r\n";
    output << "X-Hemera-Label: " << message.label_index << "\r\n";
    output << "X-Hemera-Junk-Score: " << message.junk_score << "\r\n";
    output << "X-Hemera-Manually-Junked: " << (message.manually_junked ? "1" : "0") << "\r\n";
    output << "X-Hemera-Pop-Server-Status: " << ToString(message.pop_server_status) << "\r\n";
    output << "X-Hemera-Scheduled-Send-At: " << message.scheduled_send_at << "\r\n";
    output << "X-Hemera-Priority: " << PriorityToString(message.compose_options.priority) << "\r\n";
    output << "X-Hemera-Attachment-Encoding: "
           << AttachmentEncodingToString(message.compose_options.attachment_encoding) << "\r\n";
    output << "X-Hemera-Keep-Copies: " << (message.compose_options.keep_copies ? "1" : "0") << "\r\n";
    output << "X-Hemera-Request-Read-Receipt: "
           << (message.compose_options.request_read_receipt ? "1" : "0") << "\r\n";
    output << "X-Hemera-Quoted-Printable: "
           << (message.compose_options.quoted_printable ? "1" : "0") << "\r\n";
    output << "X-Hemera-Word-Wrap: " << (message.compose_options.word_wrap ? "1" : "0") << "\r\n";
    output << "X-Hemera-Tabs-In-Body: " << (message.compose_options.tabs_in_body ? "1" : "0") << "\r\n";
    output << "X-Hemera-Text-As-Document: "
           << (message.compose_options.text_as_document ? "1" : "0") << "\r\n";
    output << "X-Hemera-Legacy-Receipt-Header: "
           << (message.use_legacy_return_receipt_header ? "1" : "0") << "\r\n";
    output << "X-Hemera-Styled-Source: " << ToString(message.styled_source) << "\r\n";
    output << "X-Hemera-Styled-Fidelity: " << ToString(message.styled_fidelity) << "\r\n";
    if (!message.remote_id.empty()) {
        output << "X-UID: " << message.remote_id << "\r\n";
        output << "X-IMFLAGS: " << BuildImapFlagsHeader(message) << "\r\n";
    }
    output << "Subject: " << message.subject << "\r\n";
    output << "From: " << message.sender << "\r\n";
    output << "To: " << message.recipients << "\r\n";
    output << "X-Priority: " << static_cast<int>(LegacyPriority(message.compose_options.priority)) << "\r\n";
    output << "Importance: " << (message.compose_options.priority == ComposePriority::kHigh ||
                                         message.compose_options.priority == ComposePriority::kHighest
                                     ? "high"
                                     : message.compose_options.priority == ComposePriority::kLow ||
                                               message.compose_options.priority == ComposePriority::kLowest
                                           ? "low"
                                           : "normal")
           << "\r\n";
    for (const auto& attachment : message.attachments) {
        output << "X-Hemera-Attachment: " << SerializeAttachment(attachment) << "\r\n";
    }
    output << "MIME-Version: 1.0\r\n";
    output << BuildStoredBodyPart(message, attachment_payloads);
    return output.str();
}

std::optional<std::string> ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool WriteFile(const std::filesystem::path& path, std::string_view contents, std::string* error_message) {
    std::error_code create_error;
    std::filesystem::create_directories(path.parent_path(), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create directory for " + path.string() + ": " + create_error.message();
        }
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        if (error_message) {
            *error_message = "Unable to write " + path.string();
        }
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(output);
}

bool AppendFile(const std::filesystem::path& path, std::string_view contents, std::string* error_message) {
    std::error_code create_error;
    std::filesystem::create_directories(path.parent_path(), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create directory for " + path.string() + ": " + create_error.message();
        }
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::app);
    if (!output.is_open()) {
        if (error_message) {
            *error_message = "Unable to append " + path.string();
        }
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(output);
}

bool LoadToc(const std::filesystem::path& toc_path, TocData* toc, std::string* error_message) {
    std::ifstream input(toc_path, std::ios::binary);
    if (!input.is_open()) {
        if (error_message) {
            *error_message = "Unable to open TOC " + toc_path.string();
        }
        return false;
    }

    TocData loaded;
    if (!ReadLittleEndian(input, &loaded.version) || loaded.version != kTocVersion) {
        if (error_message) {
            *error_message = "Unsupported TOC version in " + toc_path.string();
        }
        return false;
    }
    if (!ReadPrimitive(input, loaded.used_version.data(), loaded.used_version.size())) {
        if (error_message) {
            *error_message = "Unable to read TOC used-version bytes.";
        }
        return false;
    }
    std::array<char, kTocNameBytes> name_buffer{};
    if (!ReadFixedString(input, &name_buffer)) {
        if (error_message) {
            *error_message = "Unable to read TOC mailbox name.";
        }
        return false;
    }
    loaded.name = FixedStringView(name_buffer.data(), name_buffer.size());

    std::int16_t mailbox_type = 0;
    std::int16_t temp = 0;
    if (!ReadLittleEndian(input, &mailbox_type) || !ReadLittleEndian(input, &temp)) {
        if (error_message) {
            *error_message = "Unable to read TOC header.";
        }
        return false;
    }
    loaded.type = static_cast<LegacyMailboxType>(mailbox_type);
    loaded.group_by_subject = (temp & 0x01) != 0;
    loaded.needs_sorting = (temp & 0x02) != 0;
    loaded.show_file_browser = (temp & 0x04) != 0;
    loaded.file_browser_view_state = (temp & 0x18) >> 3;
    loaded.hide_deleted_imap = (temp & 0x20) == 0;

    std::int16_t needs_compact = 0;
    if (!ReadLittleEndian(input, &needs_compact) ||
        !ReadLittleEndian(input, &loaded.saved_left) ||
        !ReadLittleEndian(input, &loaded.saved_top) ||
        !ReadLittleEndian(input, &loaded.saved_right) ||
        !ReadLittleEndian(input, &loaded.saved_bottom)) {
        if (error_message) {
            *error_message = "Unable to read TOC positioning data.";
        }
        return false;
    }
    loaded.needs_compact = needs_compact != 0;
    for (auto& field_width : loaded.field_widths) {
        if (!ReadLittleEndian(input, &field_width)) {
            if (error_message) {
                *error_message = "Unable to read TOC column widths.";
            }
            return false;
        }
    }
    std::int16_t unread_status = 0;
    if (!ReadLittleEndian(input, &unread_status) ||
        !ReadLittleEndian(input, &loaded.next_unique_message_id) ||
        !ReadLittleEndian(input, &loaded.plugin_id) ||
        !ReadLittleEndian(input, &loaded.plugin_tag) ||
        !ReadLittleEndian(input, &loaded.splitter_pos)) {
        if (error_message) {
            *error_message = "Unable to read TOC summary metadata.";
        }
        return false;
    }
    loaded.unread_status = static_cast<UnreadStatus>(unread_status);
    if (!ReadPrimitive(input, loaded.sort_methods.data(), loaded.sort_methods.size())) {
        if (error_message) {
            *error_message = "Unable to read TOC sort columns.";
        }
        return false;
    }
    if (!ReadPrimitive(input, &loaded.ad_failure, 1) ||
        !ReadLittleEndian(input, &loaded.stored_mbx_size_plus_one)) {
        if (error_message) {
            *error_message = "Unable to read TOC compact metadata.";
        }
        return false;
    }
    std::array<char, kUnusedTocBytes> unused{};
    std::uint16_t summary_count = 0;
    if (!ReadPrimitive(input, unused.data(), unused.size()) || !ReadLittleEndian(input, &summary_count)) {
        if (error_message) {
            *error_message = "Unable to read TOC summary count.";
        }
        return false;
    }

    loaded.summaries.reserve(summary_count);
    for (std::uint16_t index = 0; index < summary_count; ++index) {
        TocSummary summary;
        char ignored_priority_padding = 0;
        char mood = 0;
        char junk = 0;
        std::int16_t timezone = 0;
        if (!ReadLittleEndian(input, &summary.offset) ||
            !ReadLittleEndian(input, &summary.length) ||
            !ReadLittleEndian(input, &summary.seconds) ||
            !ReadPrimitive(input, &summary.state, 1) ||
            !ReadLittleEndian(input, &summary.flags) ||
            !ReadPrimitive(input, &summary.priority, 1) ||
            !ReadPrimitive(input, &ignored_priority_padding, 1) ||
            !ReadFixedString(input, &summary.date) ||
            !ReadLittleEndian(input, &summary.arrival_seconds) ||
            !ReadFixedString(input, &summary.from) ||
            !ReadFixedString(input, &summary.subject) ||
            !ReadLittleEndian(input, &summary.saved_left) ||
            !ReadLittleEndian(input, &summary.saved_top) ||
            !ReadLittleEndian(input, &summary.saved_right) ||
            !ReadLittleEndian(input, &summary.saved_bottom) ||
            !ReadLittleEndian(input, &summary.label) ||
            !ReadLittleEndian(input, &summary.hash) ||
            !ReadLittleEndian(input, &summary.unique_id) ||
            !ReadLittleEndian(input, &summary.flags_ex) ||
            !ReadLittleEndian(input, &summary.persona_hash) ||
            !ReadLittleEndian(input, &timezone) ||
            !ReadLittleEndian(input, &summary.imap_flags) ||
            !ReadLittleEndian(input, &summary.message_size_kb) ||
            !ReadPrimitive(input, &mood, 1) ||
            !ReadPrimitive(input, &junk, 1) ||
            !ReadLittleEndian(input, &summary.junk_plugin_id)) {
            if (error_message) {
                *error_message = "Unable to read TOC summary record.";
            }
            return false;
        }
        summary.timezone_minutes = timezone;
        summary.mood = mood;
        summary.manually_junked = (static_cast<unsigned char>(junk) & 0x80) != 0;
        summary.junk_score = static_cast<std::uint8_t>(junk) & 0x7F;
        std::array<char, kUnusedSummaryBytes> summary_unused{};
        if (!ReadPrimitive(input, summary_unused.data(), summary_unused.size())) {
            if (error_message) {
                *error_message = "Unable to read TOC summary padding.";
            }
            return false;
        }
        loaded.summaries.push_back(summary);
    }

    *toc = std::move(loaded);
    return true;
}

bool WriteToc(const std::filesystem::path& toc_path, TocData toc, std::string* error_message) {
    std::error_code create_error;
    std::filesystem::create_directories(toc_path.parent_path(), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create TOC directory: " + create_error.message();
        }
        return false;
    }

    std::ofstream output(toc_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        if (error_message) {
            *error_message = "Unable to write TOC " + toc_path.string();
        }
        return false;
    }

    WriteLittleEndian(output, toc.version);
    output.write(reinterpret_cast<const char*>(toc.used_version.data()),
                 static_cast<std::streamsize>(toc.used_version.size()));
    WriteFixedString<kTocNameBytes>(output, toc.name);
    WriteLittleEndian(output, static_cast<std::int16_t>(toc.type));
    std::int16_t temp = 0;
    if (toc.group_by_subject) {
        temp |= 0x01;
    }
    if (toc.needs_sorting) {
        temp |= 0x02;
    }
    if (toc.show_file_browser) {
        temp |= 0x04;
    }
    temp |= static_cast<std::int16_t>((toc.file_browser_view_state & 0x03) << 3);
    if (!toc.hide_deleted_imap) {
        temp |= 0x20;
    }
    WriteLittleEndian(output, temp);
    WriteLittleEndian(output, static_cast<std::int16_t>(toc.needs_compact ? 1 : 0));
    WriteLittleEndian(output, toc.saved_left);
    WriteLittleEndian(output, toc.saved_top);
    WriteLittleEndian(output, toc.saved_right);
    WriteLittleEndian(output, toc.saved_bottom);
    for (const auto width : toc.field_widths) {
        WriteLittleEndian(output, width);
    }
    WriteLittleEndian(output, static_cast<std::int16_t>(toc.unread_status));
    WriteLittleEndian(output, toc.next_unique_message_id);
    WriteLittleEndian(output, toc.plugin_id);
    WriteLittleEndian(output, toc.plugin_tag);
    WriteLittleEndian(output, toc.splitter_pos);
    output.write(reinterpret_cast<const char*>(toc.sort_methods.data()),
                 static_cast<std::streamsize>(toc.sort_methods.size()));
    output.write(reinterpret_cast<const char*>(&toc.ad_failure), 1);
    WriteLittleEndian(output, toc.stored_mbx_size_plus_one);
    std::array<char, kUnusedTocBytes> unused{};
    output.write(unused.data(), static_cast<std::streamsize>(unused.size()));
    WriteLittleEndian(output, static_cast<std::uint16_t>(toc.summaries.size()));
    for (auto summary : toc.summaries) {
        WriteLittleEndian(output, summary.offset);
        WriteLittleEndian(output, summary.length);
        WriteLittleEndian(output, summary.seconds);
        output.write(&summary.state, 1);
        WriteLittleEndian(output, summary.flags);
        output.write(&summary.priority, 1);
        output.put('\0');
        output.write(summary.date.data(), static_cast<std::streamsize>(summary.date.size() - 1));
        output.put('\0');
        WriteLittleEndian(output, summary.arrival_seconds);
        output.write(summary.from.data(), static_cast<std::streamsize>(summary.from.size()));
        output.write(summary.subject.data(), static_cast<std::streamsize>(summary.subject.size()));
        WriteLittleEndian(output, summary.saved_left);
        WriteLittleEndian(output, summary.saved_top);
        WriteLittleEndian(output, summary.saved_right);
        WriteLittleEndian(output, summary.saved_bottom);
        WriteLittleEndian(output, summary.label);
        WriteLittleEndian(output, summary.hash);
        WriteLittleEndian(output, summary.unique_id);
        WriteLittleEndian(output, summary.flags_ex);
        WriteLittleEndian(output, summary.persona_hash);
        WriteLittleEndian(output, summary.timezone_minutes);
        WriteLittleEndian(output, summary.imap_flags);
        WriteLittleEndian(output, summary.message_size_kb);
        output.write(&summary.mood, 1);
        unsigned char junk = summary.junk_score;
        if (summary.manually_junked) {
            junk |= 0x80;
        }
        output.write(reinterpret_cast<const char*>(&junk), 1);
        WriteLittleEndian(output, summary.junk_plugin_id);
        std::array<char, kUnusedSummaryBytes> summary_unused{};
        output.write(summary_unused.data(), static_cast<std::streamsize>(summary_unused.size()));
    }
    if (!output.good()) {
        if (error_message) {
            *error_message = "Unable to finish writing TOC " + toc_path.string();
        }
        return false;
    }
    return true;
}

std::optional<LegacyMailboxType> LocalMailboxTypeFromDescmapCode(char code, std::string_view filename) {
    switch (std::toupper(static_cast<unsigned char>(code))) {
        case 'S': {
            const std::string stem = ToLower(std::filesystem::path(filename).stem().string());
            if (stem == "in") {
                return LegacyMailboxType::kIn;
            }
            if (stem == "out") {
                return LegacyMailboxType::kOut;
            }
            if (stem == "junk") {
                return LegacyMailboxType::kJunk;
            }
            if (stem == "trash") {
                return LegacyMailboxType::kTrash;
            }
            return std::nullopt;
        }
        case 'M':
            return LegacyMailboxType::kRegular;
        case 'F':
            return LegacyMailboxType::kFolder;
    }
    return std::nullopt;
}

UnreadStatus UnreadStatusFromChar(char code) {
    switch (std::toupper(static_cast<unsigned char>(code))) {
        case 'Y':
            return UnreadStatus::kYes;
        case 'N':
            return UnreadStatus::kNo;
        default:
            return UnreadStatus::kUnknown;
    }
}

char UnreadStatusCode(UnreadStatus status) {
    switch (status) {
        case UnreadStatus::kYes:
            return 'Y';
        case UnreadStatus::kNo:
            return 'N';
        case UnreadStatus::kUnknown:
        default:
            return 'U';
    }
}

char DescmapTypeCode(LegacyMailboxType type) {
    switch (type) {
        case LegacyMailboxType::kIn:
        case LegacyMailboxType::kOut:
        case LegacyMailboxType::kJunk:
        case LegacyMailboxType::kTrash:
            return 'S';
        case LegacyMailboxType::kFolder:
            return 'F';
        case LegacyMailboxType::kRegular:
        default:
            return 'M';
    }
}

std::string EscapeImapName(std::string value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char ch : value) {
        escaped.push_back(ch);
        if (ch == ',') {
            escaped.push_back(',');
        }
    }
    return escaped;
}

std::string UnescapeImapName(std::string_view value) {
    std::string unescaped;
    unescaped.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        unescaped.push_back(value[index]);
        if (value[index] == ',' && index + 1 < value.size() && value[index + 1] == ',') {
            ++index;
        }
    }
    return unescaped;
}

bool WriteDescmapForScope(const std::filesystem::path& scope_directory, std::string* error_message);
bool WriteImapMailboxList(const std::filesystem::path& account_root,
                          const std::vector<ResolvedMailbox>& mailboxes,
                          std::string* error_message);

bool LegacyReadMessageFile(const std::filesystem::path& path,
                           std::string_view mailbox_id,
                           MessageRecord* message) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    MessageRecord loaded;
    loaded.mailbox_id = std::string(mailbox_id);
    loaded.id = path.stem().string();
    bool has_explicit_legacy_status = false;
    std::string logical_id;
    std::string line;
    bool in_headers = true;
    std::ostringstream body_stream;
    while (std::getline(input, line)) {
        if (in_headers) {
            if (line.empty() || line == "\r") {
                in_headers = false;
                continue;
            }
            const std::string normalized_line = NormalizeHemeraHeaderLine(line);
            if (normalized_line.rfind("Subject:", 0) == 0) {
                loaded.subject = HeaderValue(normalized_line);
            } else if (normalized_line.rfind("From:", 0) == 0) {
                loaded.sender = HeaderValue(normalized_line);
            } else if (normalized_line.rfind("To:", 0) == 0) {
                loaded.recipients = HeaderValue(normalized_line);
            } else if (normalized_line.rfind("X-Hemera-Unique-Id:", 0) == 0 ||
                       normalized_line.rfind("X-Hemera-Message-Id:", 0) == 0) {
                loaded.id = HeaderValue(normalized_line);
            } else if (normalized_line.rfind("X-Hemera-Logical-Id:", 0) == 0) {
                logical_id = HeaderValue(normalized_line);
            } else if (normalized_line.rfind("X-Hemera-Account-Id:", 0) == 0) {
                loaded.account_id = HeaderValue(normalized_line);
            } else if (normalized_line.rfind("X-Hemera-Delivery-State:", 0) == 0) {
                loaded.delivery_state = DeliveryStateFromString(HeaderValue(normalized_line));
            } else if (normalized_line.rfind("X-Hemera-Remote-Id:", 0) == 0) {
                loaded.remote_id = HeaderValue(normalized_line);
            } else if (normalized_line.rfind("X-Hemera-Remote-Mailbox:", 0) == 0) {
                loaded.remote_mailbox = HeaderValue(normalized_line);
            } else if (normalized_line.rfind("X-Hemera-Download-Complete:", 0) == 0) {
                loaded.download_complete = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Attachments-Omitted:", 0) == 0) {
                loaded.attachments_omitted = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Flagged:", 0) == 0) {
                loaded.flagged = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Deleted:", 0) == 0) {
                loaded.deleted = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Answered:", 0) == 0) {
                loaded.answered = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Filters-Applied:", 0) == 0) {
                loaded.filters_applied = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Last-Error:", 0) == 0) {
                loaded.last_error = HeaderValue(normalized_line);
            } else if (normalized_line.rfind("X-Hemera-Created-At:", 0) == 0) {
                try {
                    loaded.created_at = std::stoll(HeaderValue(normalized_line));
                } catch (...) {
                    loaded.created_at = 0;
                }
            } else if (normalized_line.rfind("X-Hemera-Updated-At:", 0) == 0) {
                try {
                    loaded.updated_at = std::stoll(HeaderValue(normalized_line));
                } catch (...) {
                    loaded.updated_at = 0;
                }
            } else if (normalized_line.rfind("X-Hemera-Unread:", 0) == 0) {
                loaded.unread = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Legacy-Status:", 0) == 0) {
                loaded.legacy_status =
                    LegacyMessageStatusFromString(HeaderValue(normalized_line), loaded.legacy_status);
                has_explicit_legacy_status = true;
            } else if (normalized_line.rfind("X-Hemera-Label:", 0) == 0) {
                try {
                    loaded.label_index = std::stoi(HeaderValue(normalized_line));
                } catch (...) {
                    loaded.label_index = 0;
                }
            } else if (normalized_line.rfind("X-Hemera-Junk-Score:", 0) == 0) {
                try {
                    loaded.junk_score = std::stoi(HeaderValue(normalized_line));
                } catch (...) {
                    loaded.junk_score = 0;
                }
            } else if (normalized_line.rfind("X-Hemera-Manually-Junked:", 0) == 0) {
                loaded.manually_junked = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Pop-Server-Status:", 0) == 0) {
                loaded.pop_server_status =
                    PopServerStatusFromString(HeaderValue(normalized_line), loaded.pop_server_status);
            } else if (normalized_line.rfind("X-Hemera-Scheduled-Send-At:", 0) == 0) {
                try {
                    loaded.scheduled_send_at = std::stoll(HeaderValue(normalized_line));
                } catch (...) {
                    loaded.scheduled_send_at = 0;
                }
            } else if (normalized_line.rfind("X-Hemera-Priority:", 0) == 0) {
                loaded.compose_options.priority = PriorityFromString(HeaderValue(normalized_line));
            } else if (normalized_line.rfind("X-Hemera-Attachment-Encoding:", 0) == 0) {
                loaded.compose_options.attachment_encoding =
                    AttachmentEncodingFromString(HeaderValue(normalized_line));
            } else if (normalized_line.rfind("X-Hemera-Keep-Copies:", 0) == 0) {
                loaded.compose_options.keep_copies = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Request-Read-Receipt:", 0) == 0) {
                loaded.compose_options.request_read_receipt = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Quoted-Printable:", 0) == 0) {
                loaded.compose_options.quoted_printable = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Word-Wrap:", 0) == 0) {
                loaded.compose_options.word_wrap = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Tabs-In-Body:", 0) == 0) {
                loaded.compose_options.tabs_in_body = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Text-As-Document:", 0) == 0) {
                loaded.compose_options.text_as_document = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Legacy-Receipt-Header:", 0) == 0) {
                loaded.use_legacy_return_receipt_header = HeaderValue(normalized_line) != "0";
            } else if (normalized_line.rfind("X-Hemera-Styled-Source:", 0) == 0) {
                loaded.styled_source = ParseStyledDocumentSource(HeaderValue(normalized_line));
            } else if (normalized_line.rfind("X-Hemera-Styled-Fidelity:", 0) == 0) {
                loaded.styled_fidelity = ParseStyledDocumentFidelity(HeaderValue(normalized_line));
            } else if (normalized_line.rfind("X-Hemera-Attachment:", 0) == 0) {
                if (const auto attachment = ParseAttachment(HeaderValue(normalized_line))) {
                    loaded.attachments.push_back(*attachment);
                }
            }
            continue;
        }
        body_stream << line;
        if (!input.eof()) {
            body_stream << '\n';
        }
    }

    const std::string body = body_stream.str();
    constexpr std::string_view kHtmlMarker = "\n--HEMERA-HTML-BODY--\n";
    constexpr std::string_view kLegacyHtmlMarker = "\n--HERMES-HTML-BODY--\n";
    std::size_t marker = body.find(kHtmlMarker);
    std::size_t marker_length = kHtmlMarker.size();
    if (marker == std::string::npos) {
        marker = body.find(kLegacyHtmlMarker);
        marker_length = kLegacyHtmlMarker.size();
    }
    if (marker == std::string::npos) {
        loaded.plain_text_body = body;
    } else {
        loaded.plain_text_body = body.substr(0, marker);
        loaded.html_body = body.substr(marker + marker_length);
    }
    if (!logical_id.empty()) {
        loaded.id = std::move(logical_id);
    }
    if (!has_explicit_legacy_status) {
        loaded.legacy_status = DefaultLegacyStatusForMessage(loaded);
    }
    *message = std::move(loaded);
    return true;
}

std::string StyledSendModeToString(const ComposePolicy& policy) {
    if (policy.send_styled_only) {
        return "styled-only";
    }
    if (policy.send_plain_and_styled) {
        return "plain-and-styled";
    }
    return "plain-only";
}

void WritePolicy(IniSettingsStore& metadata, const ComposePolicy& policy) {
    metadata.SetString("Policy", "ReadOnly", policy.read_only ? "1" : "0");
    metadata.SetString("Policy", "AllowStyled", policy.allow_styled ? "1" : "0");
    metadata.SetString("Policy", "SendPlainOnly", policy.send_plain_only ? "1" : "0");
    metadata.SetString("Policy", "WarnOnStyledSend", policy.warn_on_styled_send ? "1" : "0");
    metadata.SetString("Policy", "DefaultSignatureName", policy.default_signature_name);
    metadata.SetString("Policy", "DefaultStationeryName", policy.default_stationery_name);
    metadata.SetString("Policy", "UserSignaturesEnabled", policy.user_signatures_enabled ? "1" : "0");
    metadata.SetString("Policy", "MoodWatchEnabled", policy.mood_watch_enabled ? "1" : "0");
    metadata.SetString("Policy", "MoodCheckBackground", policy.mood_check_background ? "1" : "0");
    metadata.SetString("Policy", "MoodWatchIntervalMs", std::to_string(policy.mood_watch_interval_ms));
    metadata.SetString("Policy", "MoodWarnWhenMightOffend", policy.mood_warn_when_might_offend ? "1" : "0");
    metadata.SetString("Policy",
                       "MoodWarnWhenProbablyOffend",
                       policy.mood_warn_when_probably_offend ? "1" : "0");
    metadata.SetString("Policy", "MoodWarnWhenOnFire", policy.mood_warn_when_on_fire ? "1" : "0");
    metadata.SetString("Policy", "MoodShowCompBadWords", policy.mood_show_comp_bad_words ? "1" : "0");
    metadata.SetString("Policy", "AutoCheckSpelling", policy.auto_check_spelling ? "1" : "0");
    metadata.SetString("Policy", "SpellCheckIntervalMs", std::to_string(policy.spell_check_interval_ms));
    metadata.SetString("Policy",
                       "BossProtectorIntervalMs",
                       std::to_string(policy.boss_protector_interval_ms));
    metadata.SetString("Policy",
                       "BossProtectorWarnOutsideDomains",
                       policy.boss_protector_warn_outside_domains ? "1" : "0");
    metadata.SetString("Policy", "BossProtectorOutsideDomains", policy.boss_protector_outside_domains);
    metadata.SetString("Policy",
                       "BossProtectorWarnInsideDomains",
                       policy.boss_protector_warn_inside_domains ? "1" : "0");
    metadata.SetString("Policy", "BossProtectorInsideDomains", policy.boss_protector_inside_domains);
    metadata.SetString("Policy",
                       "BossProtectorAdditionalWarnDialog",
                       policy.boss_protector_additional_warn_dialog ? "1" : "0");
    metadata.SetString("Policy", "StyledSendMode", StyledSendModeToString(policy));
    metadata.SetString("Policy", "DefaultPriority", PriorityToString(policy.default_options.priority));
    metadata.SetString("Policy",
                       "DefaultAttachmentEncoding",
                       AttachmentEncodingToString(policy.default_options.attachment_encoding));
    metadata.SetString("Policy", "DefaultKeepCopies", policy.default_options.keep_copies ? "1" : "0");
    metadata.SetString("Policy",
                       "DefaultRequestReadReceipt",
                       policy.default_options.request_read_receipt ? "1" : "0");
    metadata.SetString("Policy",
                       "DefaultQuotedPrintable",
                       policy.default_options.quoted_printable ? "1" : "0");
    metadata.SetString("Policy", "DefaultWordWrap", policy.default_options.word_wrap ? "1" : "0");
    metadata.SetString("Policy", "DefaultTabsInBody", policy.default_options.tabs_in_body ? "1" : "0");
    metadata.SetString("Policy",
                       "DefaultTextAsDocument",
                       policy.default_options.text_as_document ? "1" : "0");
    metadata.SetString("Policy",
                       "ReturnReceiptLegacyHeader",
                       policy.return_receipt_legacy_header ? "1" : "0");
    metadata.SetString("Policy", "WordWrapOnScreen", policy.word_wrap_on_screen ? "1" : "0");
    metadata.SetString("Policy", "WordWrapColumn", std::to_string(policy.word_wrap_column));
    metadata.SetString("Policy", "WordWrapMax", std::to_string(policy.word_wrap_max));
}

ComposePolicy ReadPolicy(const IniSettingsStore& metadata) {
    ComposePolicy policy;
    policy.read_only = metadata.GetBool("Policy", "ReadOnly", false);
    policy.allow_styled = metadata.GetBool("Policy", "AllowStyled", true);
    policy.send_plain_only = metadata.GetBool("Policy", "SendPlainOnly", false);
    policy.warn_on_styled_send = metadata.GetBool("Policy", "WarnOnStyledSend", false);
    policy.default_signature_name = metadata.GetString("Policy", "DefaultSignatureName").value_or("");
    policy.default_stationery_name = metadata.GetString("Policy", "DefaultStationeryName").value_or("");
    policy.user_signatures_enabled = metadata.GetBool("Policy", "UserSignaturesEnabled", false);
    policy.mood_watch_enabled = metadata.GetBool("Policy", "MoodWatchEnabled", true);
    policy.mood_check_background = metadata.GetBool("Policy", "MoodCheckBackground", true);
    policy.mood_watch_interval_ms = metadata.GetInt("Policy", "MoodWatchIntervalMs", 1000);
    policy.mood_warn_when_might_offend = metadata.GetBool("Policy", "MoodWarnWhenMightOffend", false);
    policy.mood_warn_when_probably_offend =
        metadata.GetBool("Policy", "MoodWarnWhenProbablyOffend", true);
    policy.mood_warn_when_on_fire = metadata.GetBool("Policy", "MoodWarnWhenOnFire", false);
    policy.mood_show_comp_bad_words = metadata.GetBool("Policy", "MoodShowCompBadWords", true);
    policy.auto_check_spelling = metadata.GetBool("Policy", "AutoCheckSpelling", true);
    policy.spell_check_interval_ms = metadata.GetInt("Policy", "SpellCheckIntervalMs", 500);
    policy.boss_protector_interval_ms = metadata.GetInt("Policy", "BossProtectorIntervalMs", 500);
    policy.boss_protector_warn_outside_domains =
        metadata.GetBool("Policy", "BossProtectorWarnOutsideDomains", false);
    policy.boss_protector_outside_domains =
        metadata.GetString("Policy", "BossProtectorOutsideDomains").value_or("");
    policy.boss_protector_warn_inside_domains =
        metadata.GetBool("Policy", "BossProtectorWarnInsideDomains", false);
    policy.boss_protector_inside_domains =
        metadata.GetString("Policy", "BossProtectorInsideDomains").value_or("");
    policy.boss_protector_additional_warn_dialog =
        metadata.GetBool("Policy", "BossProtectorAdditionalWarnDialog", false);
    const std::string mode = metadata.GetString("Policy", "StyledSendMode").value_or("plain-only");
    policy.send_styled_only = mode == "styled-only";
    policy.send_plain_and_styled = mode == "plain-and-styled";
    if (!policy.send_styled_only && !policy.send_plain_and_styled) {
        policy.send_plain_only = true;
    }
    policy.default_options.priority =
        PriorityFromString(metadata.GetString("Policy", "DefaultPriority").value_or("normal"));
    policy.default_options.attachment_encoding = AttachmentEncodingFromString(
        metadata.GetString("Policy", "DefaultAttachmentEncoding").value_or("mime"));
    policy.default_options.keep_copies = metadata.GetBool("Policy", "DefaultKeepCopies", false);
    policy.default_options.request_read_receipt =
        metadata.GetBool("Policy", "DefaultRequestReadReceipt", false);
    policy.default_options.quoted_printable =
        metadata.GetBool("Policy", "DefaultQuotedPrintable", true);
    policy.default_options.word_wrap = metadata.GetBool("Policy", "DefaultWordWrap", true);
    policy.default_options.tabs_in_body = metadata.GetBool("Policy", "DefaultTabsInBody", true);
    policy.default_options.text_as_document =
        metadata.GetBool("Policy", "DefaultTextAsDocument", false);
    policy.return_receipt_legacy_header =
        metadata.GetBool("Policy", "ReturnReceiptLegacyHeader", false);
    policy.word_wrap_on_screen = metadata.GetBool("Policy", "WordWrapOnScreen", false);
    policy.word_wrap_column = metadata.GetInt("Policy", "WordWrapColumn", 70);
    policy.word_wrap_max = metadata.GetInt("Policy", "WordWrapMax", 80);
    return policy;
}

void WriteOptions(IniSettingsStore& metadata, const ComposeOptions& options) {
    metadata.SetString("Options", "Priority", PriorityToString(options.priority));
    metadata.SetString("Options",
                       "AttachmentEncoding",
                       AttachmentEncodingToString(options.attachment_encoding));
    metadata.SetString("Options", "KeepCopies", options.keep_copies ? "1" : "0");
    metadata.SetString("Options", "RequestReadReceipt", options.request_read_receipt ? "1" : "0");
    metadata.SetString("Options", "QuotedPrintable", options.quoted_printable ? "1" : "0");
    metadata.SetString("Options", "WordWrap", options.word_wrap ? "1" : "0");
    metadata.SetString("Options", "TabsInBody", options.tabs_in_body ? "1" : "0");
    metadata.SetString("Options", "TextAsDocument", options.text_as_document ? "1" : "0");
}

ComposeOptions ReadOptions(const IniSettingsStore& metadata, const ComposePolicy& policy) {
    ComposeOptions options = policy.default_options;
    options.priority =
        PriorityFromString(metadata.GetString("Options", "Priority").value_or(PriorityToString(options.priority)));
    options.attachment_encoding = AttachmentEncodingFromString(
        metadata.GetString("Options", "AttachmentEncoding")
            .value_or(AttachmentEncodingToString(options.attachment_encoding)));
    options.keep_copies = metadata.GetBool("Options", "KeepCopies", options.keep_copies);
    options.request_read_receipt =
        metadata.GetBool("Options", "RequestReadReceipt", options.request_read_receipt);
    options.quoted_printable =
        metadata.GetBool("Options", "QuotedPrintable", options.quoted_printable);
    options.word_wrap = metadata.GetBool("Options", "WordWrap", options.word_wrap);
    options.tabs_in_body = metadata.GetBool("Options", "TabsInBody", options.tabs_in_body);
    options.text_as_document =
        metadata.GetBool("Options", "TextAsDocument", options.text_as_document);
    return options;
}

std::filesystem::path DraftSidecarPath(const std::filesystem::path& root_directory, std::string_view draft_id) {
    return DraftSidecarRoot(root_directory) / (SanitizePathComponent(std::string(draft_id)) + ".ini");
}

std::filesystem::path DraftSidecarDataPath(const std::filesystem::path& root_directory,
                                           std::string_view draft_id,
                                           std::string_view extension) {
    return DraftSidecarRoot(root_directory) /
           (SanitizePathComponent(std::string(draft_id)) + "." + std::string(extension));
}

bool SaveDraftSidecar(const std::filesystem::path& root_directory,
                     const ComposeMessage& draft,
                     std::string* error_message) {
    const RichTextDocument prepared_body = PrepareRichTextDocumentForPersistence(draft.body);
    IniSettingsStore metadata;
    metadata.SetString("Draft", "Id", draft.id);
    metadata.SetString("Draft", "StationeryName", draft.stationery_name);
    metadata.SetString("Draft", "SignatureName", draft.signature_name);
    metadata.SetString("Draft", "ManagedSignatureAttached", draft.managed_signature.attached ? "1" : "0");
    metadata.SetString("Draft", "ManagedSignatureName", draft.managed_signature.name);
    metadata.SetString("Draft", "ManagedSignatureStart", std::to_string(draft.managed_signature.start));
    metadata.SetString("Draft", "ManagedSignatureLength", std::to_string(draft.managed_signature.length));
    metadata.SetString("Draft", "ManagedSignaturePlainText", draft.managed_signature.plain_text);
    metadata.SetString("Headers", "To", draft.headers.to);
    metadata.SetString("Headers", "Cc", draft.headers.cc);
    metadata.SetString("Headers", "Bcc", draft.headers.bcc);
    metadata.SetString("Headers", "Subject", draft.headers.subject);
    metadata.SetString("Headers", "FromPersona", draft.headers.from_persona);
    metadata.SetString("Headers", "ReplyTo", draft.headers.reply_to);
    metadata.SetString("Body", "StyledSource", ToString(prepared_body.styled_source));
    metadata.SetString("Body", "StyledFidelity", ToString(prepared_body.fidelity));
    metadata.SetString("Attachments", "Count", std::to_string(draft.attachments.size()));
    for (std::size_t index = 0; index < draft.attachments.size(); ++index) {
        const auto& attachment = draft.attachments[index];
        const std::string prefix = "Attachment" + std::to_string(index);
        metadata.SetString("Attachments", prefix + "DisplayName", attachment.display_name);
        metadata.SetString("Attachments", prefix + "MimeType", attachment.mime_type);
        metadata.SetString("Attachments", prefix + "Size", std::to_string(attachment.size));
        metadata.SetString("Attachments", prefix + "ContentId", attachment.content_id);
        metadata.SetString("Attachments",
                           prefix + "InlineDisposition",
                           attachment.inline_disposition ? "1" : "0");
    }
    WritePolicy(metadata, draft.policy);
    WriteOptions(metadata, draft.options);
    if (!metadata.SaveToFile(DraftSidecarPath(root_directory, draft.id), error_message)) {
        return false;
    }
    std::error_code create_error;
    std::filesystem::create_directories(DraftSidecarRoot(root_directory), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create draft sidecar directory: " + create_error.message();
        }
        return false;
    }
    auto write_blob = [&](std::string_view extension, const std::string& value) {
        std::ofstream output(DraftSidecarDataPath(root_directory, draft.id, extension), std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            if (error_message) {
                *error_message = "Unable to write draft sidecar data.";
            }
            return false;
        }
        output.write(value.data(), static_cast<std::streamsize>(value.size()));
        return static_cast<bool>(output);
    };
    return write_blob("html", prepared_body.html_fragment) && write_blob("rtf", prepared_body.rtf_fragment) &&
           write_blob("pg", prepared_body.paige_native_bytes);
}

std::optional<IniSettingsStore> LoadDraftSidecar(const std::filesystem::path& root_directory, std::string_view draft_id) {
    IniSettingsStore metadata;
    std::string ignored;
    if (!metadata.LoadFromFile(DraftSidecarPath(root_directory, draft_id), &ignored)) {
        return std::nullopt;
    }
    return metadata;
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

std::string JoinRecipients(const ComposeHeaders& headers) {
    std::string recipients = headers.to;
    if (!headers.cc.empty()) {
        if (!recipients.empty()) {
            recipients += ", ";
        }
        recipients += headers.cc;
    }
    if (!headers.bcc.empty()) {
        if (!recipients.empty()) {
            recipients += ", ";
        }
        recipients += headers.bcc;
    }
    return recipients;
}

MessageRecord MessageRecordFromDraft(const ComposeMessage& draft) {
    const RichTextDocument prepared_body = PrepareRichTextDocumentForPersistence(draft.body);
    MessageRecord message;
    message.id = draft.id;
    message.mailbox_id = "drafts";
    message.account_id = draft.headers.from_persona.empty() ? "primary" : draft.headers.from_persona;
    message.subject = draft.headers.subject;
    message.sender = draft.headers.from_persona;
    message.recipients = JoinRecipients(draft.headers);
    message.plain_text_body = prepared_body.plain_text;
    message.html_body = prepared_body.html_fragment;
    message.rtf_body = prepared_body.rtf_fragment;
    message.paige_native_body = prepared_body.paige_native_bytes;
    message.styled_source = prepared_body.styled_source;
    message.styled_fidelity = prepared_body.fidelity;
    message.delivery_state = MessageDeliveryState::kDraft;
    message.legacy_status = LegacyMessageStatus::kUnsent;
    message.created_at = NowUnixSeconds();
    message.updated_at = message.created_at;
    message.unread = false;
    message.compose_options = draft.options;
    message.use_legacy_return_receipt_header = draft.policy.return_receipt_legacy_header;
    for (const auto& attachment : draft.attachments) {
        MessageAttachment stored_attachment;
        stored_attachment.name =
            attachment.display_name.empty() ? attachment.source_path.filename().string() : attachment.display_name;
        stored_attachment.content_type = attachment.mime_type;
        stored_attachment.size = static_cast<std::size_t>(attachment.size);
        stored_attachment.payload_path = attachment.source_path.string();
        stored_attachment.content_id = attachment.content_id;
        stored_attachment.disposition = attachment.inline_disposition ? "inline" : "attachment";
        stored_attachment.download_complete = true;
        message.attachments.push_back(std::move(stored_attachment));
    }
    return message;
}

ComposeMessage ComposeMessageFromDraft(const std::filesystem::path& root_directory,
                                      const MessageRecord& message,
                                      const std::optional<IniSettingsStore>& sidecar) {
    ComposeMessage draft;
    draft.id = message.id;
    draft.body.plain_text = message.plain_text_body;
    draft.body.html_fragment = message.html_body;
    draft.body.rtf_fragment = message.rtf_body;
    draft.body.paige_native_bytes = message.paige_native_body;
    draft.body.styled_source = message.styled_source;
    draft.body.fidelity = message.styled_fidelity;
    draft.body.read_only = false;
    draft.options = message.compose_options;
    draft.policy.return_receipt_legacy_header = message.use_legacy_return_receipt_header;
    draft.headers.subject = message.subject;
    draft.headers.from_persona = message.sender;
    draft.headers.to = message.recipients;
    for (const auto& attachment : message.attachments) {
        ComposeAttachment compose_attachment;
        compose_attachment.display_name = attachment.name;
        compose_attachment.source_path = attachment.payload_path;
        compose_attachment.mime_type = attachment.content_type;
        compose_attachment.size = static_cast<std::uint64_t>(attachment.size);
        compose_attachment.content_id = attachment.content_id;
        compose_attachment.inline_disposition = attachment.disposition == "inline";
        draft.attachments.push_back(std::move(compose_attachment));
    }
    if (sidecar) {
        draft.id = sidecar->GetString("Draft", "Id").value_or(draft.id);
        draft.stationery_name = sidecar->GetString("Draft", "StationeryName").value_or("");
        draft.signature_name = sidecar->GetString("Draft", "SignatureName").value_or("");
        draft.managed_signature.attached = sidecar->GetBool("Draft", "ManagedSignatureAttached", false);
        draft.managed_signature.name = sidecar->GetString("Draft", "ManagedSignatureName").value_or("");
        draft.managed_signature.start =
            static_cast<std::size_t>(sidecar->GetInt("Draft", "ManagedSignatureStart", 0));
        draft.managed_signature.length =
            static_cast<std::size_t>(sidecar->GetInt("Draft", "ManagedSignatureLength", 0));
        draft.managed_signature.plain_text =
            sidecar->GetString("Draft", "ManagedSignaturePlainText").value_or("");
        draft.headers.to = sidecar->GetString("Headers", "To").value_or(draft.headers.to);
        draft.headers.cc = sidecar->GetString("Headers", "Cc").value_or("");
        draft.headers.bcc = sidecar->GetString("Headers", "Bcc").value_or("");
        draft.headers.subject = sidecar->GetString("Headers", "Subject").value_or(draft.headers.subject);
        draft.headers.from_persona =
            sidecar->GetString("Headers", "FromPersona").value_or(draft.headers.from_persona);
        draft.headers.reply_to = sidecar->GetString("Headers", "ReplyTo").value_or("");
        draft.body.styled_source = ParseStyledDocumentSource(
            sidecar->GetString("Body", "StyledSource").value_or(ToString(draft.body.styled_source)));
        draft.body.fidelity = ParseStyledDocumentFidelity(
            sidecar->GetString("Body", "StyledFidelity").value_or(ToString(draft.body.fidelity)));
        draft.policy = ReadPolicy(*sidecar);
        draft.options = ReadOptions(*sidecar, draft.policy);
        for (std::size_t index = 0; index < draft.attachments.size(); ++index) {
            const std::string prefix = "Attachment" + std::to_string(index);
            draft.attachments[index].display_name =
                sidecar->GetString("Attachments", prefix + "DisplayName").value_or(draft.attachments[index].display_name);
            draft.attachments[index].mime_type =
                sidecar->GetString("Attachments", prefix + "MimeType").value_or(draft.attachments[index].mime_type);
            draft.attachments[index].size = static_cast<std::uint64_t>(
                std::max(sidecar->GetInt("Attachments", prefix + "Size", static_cast<int>(draft.attachments[index].size)), 0));
            draft.attachments[index].content_id =
                sidecar->GetString("Attachments", prefix + "ContentId").value_or(draft.attachments[index].content_id);
            draft.attachments[index].inline_disposition =
                sidecar->GetBool("Attachments", prefix + "InlineDisposition", false);
        }
    }
    if (std::ifstream html_input(DraftSidecarDataPath(root_directory, draft.id, "html"), std::ios::binary);
        html_input.is_open()) {
        draft.body.html_fragment =
            std::string(std::istreambuf_iterator<char>(html_input), std::istreambuf_iterator<char>());
    }
    if (std::ifstream rtf_input(DraftSidecarDataPath(root_directory, draft.id, "rtf"), std::ios::binary);
        rtf_input.is_open()) {
        draft.body.rtf_fragment =
            std::string(std::istreambuf_iterator<char>(rtf_input), std::istreambuf_iterator<char>());
    }
    if (std::ifstream native_input(DraftSidecarDataPath(root_directory, draft.id, "pg"), std::ios::binary);
        native_input.is_open()) {
        draft.body.paige_native_bytes =
            std::string(std::istreambuf_iterator<char>(native_input), std::istreambuf_iterator<char>());
    }
    draft.body = NormalizeRichTextDocument(draft.body);
    return draft;
}

std::filesystem::path ImapAccountRoot(const std::filesystem::path& root_directory, std::string_view account_id) {
    return ImapRoot(root_directory) / SanitizePathComponent(std::string(account_id));
}

std::string LocalRegularMailboxStem(const MailboxRecord& mailbox) {
    if (const auto system = SystemMailboxStem(mailbox.id)) {
        return *system;
    }
    return SanitizePathComponent(mailbox.id.empty() ? mailbox.display_name : mailbox.id);
}

std::optional<TocData> LoadOrRebuildToc(const ResolvedMailbox& mailbox, std::string* error_message);
std::vector<StoredMessageEntry> LoadMessageEntries(const std::filesystem::path& root_directory,
                                                   const ResolvedMailbox& mailbox,
                                                   std::string* error_message = nullptr);
std::optional<ResolvedMailbox> ResolveMailboxInternal(const std::filesystem::path& root_directory,
                                                      std::string_view mailbox_id);

TocSummary BuildSummaryFromMessage(const MessageRecord& message,
                                   std::uint32_t unique_id,
                                   std::int32_t offset,
                                   std::int32_t length) {
    TocSummary summary;
    summary.offset = offset;
    summary.length = length;
    summary.seconds = static_cast<std::int32_t>(message.updated_at != 0 ? message.updated_at : message.created_at);
    summary.arrival_seconds = static_cast<std::int32_t>(message.created_at != 0 ? message.created_at : summary.seconds);
    summary.state = SummaryStateForLegacyStatus(message.legacy_status);
    summary.flags = 0;
    if (message.compose_options.word_wrap) {
        summary.flags |= kMsfWordWrap;
    }
    if (message.compose_options.tabs_in_body) {
        summary.flags |= kMsfTabsInBody;
    }
    if (message.compose_options.keep_copies) {
        summary.flags |= kMsfKeepCopies;
    }
    if (message.compose_options.text_as_document) {
        summary.flags |= kMsfTextAsDoc;
    }
    if (message.use_legacy_return_receipt_header) {
        summary.flags |= kMsfReturnReceipt;
    }
    if (message.compose_options.request_read_receipt) {
        summary.flags |= kMsfReadReceipt;
        summary.flags_ex |= kMsfExMdn;
    }
    if (message.compose_options.quoted_printable) {
        summary.flags |= kMsfQuotedPrintable;
        summary.flags_ex |= kMsfExFlowed;
    }
    switch (message.compose_options.attachment_encoding) {
        case AttachmentEncodingMode::kMime:
            summary.flags |= kMsfMime;
            break;
        case AttachmentEncodingMode::kUuencode:
            summary.flags |= kMsfUuencode;
            break;
        case AttachmentEncodingMode::kBinHex:
            break;
    }
    if (!message.attachments.empty()) {
        summary.flags |= kMsfHasAttachment;
    }
    if (message.styled_source != StyledDocumentSource::kPlainText && !message.html_body.empty()) {
        summary.flags |= kMsfXrich;
        summary.flags_ex |= kMsfExHtml;
        summary.flags_ex |= kMsfExSendStyled;
        summary.flags_ex |= kMsfExSendPlain;
    }
    if (message.plain_text_body.empty() && message.html_body.empty()) {
        summary.flags_ex |= kMsfExEmptyBody;
    }
    summary.priority = LegacyPriority(message.compose_options.priority);
    summary.hash = static_cast<std::int32_t>(
        message.remote_id.empty() ? Fnv1a(message.subject + "|" + message.sender + "|" + message.recipients)
                                  : Fnv1a(message.remote_id));
    summary.unique_id = static_cast<std::int32_t>(unique_id);
    summary.persona_hash = static_cast<std::int32_t>(Fnv1a(message.account_id));
    const std::string date = FormatSummaryDate(summary.seconds);
    std::copy_n(date.c_str(), std::min(date.size(), summary.date.size() - 1), summary.date.data());
    std::copy_n(message.sender.c_str(),
                std::min(message.sender.size(), summary.from.size() - 1),
                summary.from.data());
    std::copy_n(message.subject.c_str(),
                std::min(message.subject.size(), summary.subject.size() - 1),
                summary.subject.data());
    summary.timezone_minutes = 0;
    summary.label = static_cast<std::int16_t>(std::clamp(message.label_index, 0, 7));
    summary.imap_flags = 0;
    if (!message.unread) {
        summary.imap_flags |= kImapSeen;
    }
    if (message.answered) {
        summary.imap_flags |= kImapAnswered;
    }
    if (message.flagged) {
        summary.imap_flags |= kImapFlagged;
    }
    if (message.deleted) {
        summary.imap_flags |= kImapDeleted;
    }
    if (message.delivery_state == MessageDeliveryState::kDraft) {
        summary.imap_flags |= kImapDraft;
    }
    if (message.download_complete) {
        summary.imap_flags |= kImapFullHeader;
    } else {
        summary.imap_flags |= kImapNotDownloaded;
    }
    const std::size_t undownloaded_attachments =
        std::count_if(message.attachments.begin(), message.attachments.end(), [](const MessageAttachment& attachment) {
            return attachment.omitted || !attachment.download_complete;
        });
    summary.imap_flags |= static_cast<std::uint32_t>(
        std::min<std::size_t>(undownloaded_attachments, 31) << kImapUndownloadedShift);
    summary.message_size_kb =
        static_cast<std::uint16_t>(std::max<std::int32_t>((length / 1024) + 1, 1));
    summary.junk_score = static_cast<std::uint8_t>(std::clamp(message.junk_score, 0, 127));
    summary.manually_junked = message.manually_junked;
    return summary;
}

UnreadStatus ComputeUnreadStatus(const std::vector<TocSummary>& summaries) {
    return std::any_of(summaries.begin(), summaries.end(), [](const TocSummary& summary) {
               return summary.state == kStateUnread;
           })
               ? UnreadStatus::kYes
               : UnreadStatus::kNo;
}

bool ParseStoredMessage(const std::string& raw_message,
                        std::string_view mailbox_id,
                        ParsedStoredMessage* parsed) {
    ParsedStoredMessage result;
    result.record.mailbox_id = std::string(mailbox_id);
    std::istringstream stream(raw_message);
    std::string line;
    bool in_headers = true;
    std::map<std::string, std::string> headers;
    std::ostringstream body_stream;
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
                std::string key = NormalizeHemeraHeaderKey(ToLower(Trim(line.substr(0, colon))));
                const std::string value = Trim(line.substr(colon + 1));
                headers[key] = value;
                if (StartsWithInsensitive(key, "x-hemera-")) {
                    headers["x-hermes-" + key.substr(std::string("x-hemera-").size())] = value;
                }
            }
            const std::string normalized_line = NormalizeHemeraHeaderLine(line);
            if (StartsWithInsensitive(normalized_line, "X-Hemera-Attachment:")) {
                if (const auto attachment = ParseAttachment(HeaderValue(normalized_line))) {
                    result.record.attachments.push_back(*attachment);
                }
            }
            continue;
        }
        body_stream << line;
        if (!stream.eof()) {
            body_stream << '\n';
        }
    }

    result.record.id = headers.count("x-hermes-unique-id") ? headers["x-hermes-unique-id"] : "";
    result.logical_id = headers.count("x-hermes-logical-id") ? headers["x-hermes-logical-id"] : "";
    result.draft_id = result.logical_id;
    result.record.account_id = headers.count("x-hermes-account-id") ? headers["x-hermes-account-id"] : "";
    result.record.delivery_state = DeliveryStateFromString(
        headers.count("x-hermes-delivery-state") ? headers["x-hermes-delivery-state"] : "received");
    result.record.remote_id = headers.count("x-hermes-remote-id") ? headers["x-hermes-remote-id"] : "";
    result.record.remote_mailbox = headers.count("x-hermes-remote-mailbox") ? headers["x-hermes-remote-mailbox"] : "";
    result.record.download_complete =
        !headers.count("x-hermes-download-complete") || headers["x-hermes-download-complete"] != "0";
    result.record.attachments_omitted =
        headers.count("x-hermes-attachments-omitted") && headers["x-hermes-attachments-omitted"] != "0";
    result.record.flagged = headers.count("x-hermes-flagged") && headers["x-hermes-flagged"] != "0";
    result.record.deleted = headers.count("x-hermes-deleted") && headers["x-hermes-deleted"] != "0";
    result.record.answered = headers.count("x-hermes-answered") && headers["x-hermes-answered"] != "0";
    result.record.filters_applied =
        headers.count("x-hermes-filters-applied") && headers["x-hermes-filters-applied"] != "0";
    result.record.last_error = headers.count("x-hermes-last-error") ? headers["x-hermes-last-error"] : "";
    try {
        result.record.created_at =
            headers.count("x-hermes-created-at") ? std::stoll(headers["x-hermes-created-at"]) : 0;
    } catch (...) {
        result.record.created_at = 0;
    }
    try {
        result.record.updated_at =
            headers.count("x-hermes-updated-at") ? std::stoll(headers["x-hermes-updated-at"]) : 0;
    } catch (...) {
        result.record.updated_at = 0;
    }
    result.record.unread = !headers.count("x-hermes-unread") || headers["x-hermes-unread"] != "0";
    result.has_explicit_legacy_status = headers.count("x-hermes-legacy-status") > 0;
    result.record.legacy_status = result.has_explicit_legacy_status
                                      ? LegacyMessageStatusFromString(headers["x-hermes-legacy-status"],
                                                                     LegacyMessageStatus::kUnread)
                                      : LegacyMessageStatus::kUnread;
    result.has_explicit_label = headers.count("x-hermes-label") > 0;
    if (result.has_explicit_label) {
        try {
            result.record.label_index = std::stoi(headers["x-hermes-label"]);
        } catch (...) {
            result.record.label_index = 0;
        }
    }
    result.has_explicit_junk_score = headers.count("x-hermes-junk-score") > 0;
    if (result.has_explicit_junk_score) {
        try {
            result.record.junk_score = std::stoi(headers["x-hermes-junk-score"]);
        } catch (...) {
            result.record.junk_score = 0;
        }
    }
    result.has_explicit_manually_junked = headers.count("x-hermes-manually-junked") > 0;
    if (result.has_explicit_manually_junked) {
        result.record.manually_junked = headers["x-hermes-manually-junked"] != "0";
    }
    result.has_explicit_pop_server_status = headers.count("x-hermes-pop-server-status") > 0;
    if (result.has_explicit_pop_server_status) {
        result.record.pop_server_status =
            PopServerStatusFromString(headers["x-hermes-pop-server-status"], PopServerStatus::kNone);
    }
    result.record.compose_options.priority = PriorityFromString(
        headers.count("x-hermes-priority") ? headers["x-hermes-priority"] : "normal");
    result.record.compose_options.attachment_encoding = AttachmentEncodingFromString(
        headers.count("x-hermes-attachment-encoding") ? headers["x-hermes-attachment-encoding"] : "mime");
    result.record.compose_options.keep_copies =
        headers.count("x-hermes-keep-copies") && headers["x-hermes-keep-copies"] != "0";
    result.record.compose_options.request_read_receipt =
        headers.count("x-hermes-request-read-receipt") &&
        headers["x-hermes-request-read-receipt"] != "0";
    result.record.compose_options.quoted_printable =
        !headers.count("x-hermes-quoted-printable") || headers["x-hermes-quoted-printable"] != "0";
    result.record.compose_options.word_wrap =
        !headers.count("x-hermes-word-wrap") || headers["x-hermes-word-wrap"] != "0";
    result.record.compose_options.tabs_in_body =
        !headers.count("x-hermes-tabs-in-body") || headers["x-hermes-tabs-in-body"] != "0";
    result.record.compose_options.text_as_document =
        headers.count("x-hermes-text-as-document") && headers["x-hermes-text-as-document"] != "0";
    result.record.use_legacy_return_receipt_header =
        headers.count("x-hermes-legacy-receipt-header") &&
        headers["x-hermes-legacy-receipt-header"] != "0";
    const bool has_explicit_styled_source = headers.count("x-hermes-styled-source") > 0;
    result.record.styled_source = ParseStyledDocumentSource(
        has_explicit_styled_source ? headers["x-hermes-styled-source"] : "plain");
    result.record.styled_fidelity = ParseStyledDocumentFidelity(
        headers.count("x-hermes-styled-fidelity") ? headers["x-hermes-styled-fidelity"] : "lossless");
    result.record.subject = headers.count("subject") ? headers["subject"] : "";
    result.record.sender = headers.count("from") ? headers["from"] : "";
    result.record.recipients = headers.count("to") ? headers["to"] : "";
    result.reply_to = headers.count("reply-to") ? headers["reply-to"] : "";
    if (!result.has_explicit_legacy_status) {
        result.record.legacy_status = DefaultLegacyStatusForMessage(result.record);
    }

    const std::string body = body_stream.str();
    const std::string content_type = headers.count("content-type") ? ToLower(headers["content-type"]) : "text/plain";
    if (content_type.find("multipart/") == std::string::npos) {
        if (content_type.find("text/html") != std::string::npos) {
            result.record.html_body = DecodeTransferEncodedBody(headers, body);
            result.record.plain_text_body = StripHtml(result.record.html_body);
            if (!has_explicit_styled_source) {
                result.record.styled_source = StyledDocumentSource::kHtml;
            }
        } else if (content_type.find("text/rtf") != std::string::npos ||
                   content_type.find("application/rtf") != std::string::npos) {
            result.record.rtf_body = DecodeTransferEncodedBody(headers, body);
            result.record.plain_text_body = StripRtf(result.record.rtf_body);
            if (!has_explicit_styled_source) {
                result.record.styled_source = StyledDocumentSource::kRtf;
            }
        } else if (IsHemeraPaigeMimeType(content_type)) {
            result.record.paige_native_body = Base64Decode(body);
            if (!has_explicit_styled_source) {
                result.record.styled_source = StyledDocumentSource::kPaigeNative;
            }
        } else {
            result.record.plain_text_body = DecodeTransferEncodedBody(headers, body);
        }
    } else {
        const std::string boundary = ParameterValue(headers["content-type"], "boundary");
        if (boundary.empty()) {
            result.record.plain_text_body = body;
        } else {
            const std::string delimiter = "--" + boundary;
            std::size_t offset = 0;
            std::size_t attachment_index = 0;
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
                const std::string part =
                    body.substr(content_begin, next == std::string::npos ? std::string::npos : next - content_begin);
                offset = next == std::string::npos ? body.size() : next;

                const auto [part_headers, part_body] = ParsePart(part);
                const std::string part_type = ToLower(part_headers.count("content-type")
                                                          ? part_headers.at("content-type")
                                                          : "text/plain");
                const std::string disposition =
                    part_headers.count("content-disposition") ? part_headers.at("content-disposition") : "";
                const std::string decoded_body = DecodeTransferEncodedBody(part_headers, part_body);
                const bool is_attachment =
                    disposition.find("attachment") != std::string::npos ||
                    !ParameterValue(disposition, "filename").empty() ||
                    !ParameterValue(part_headers.count("content-type") ? part_headers.at("content-type") : "", "name")
                         .empty();
                if (is_attachment) {
                    if (attachment_index >= result.record.attachments.size()) {
                        MessageAttachment attachment;
                        attachment.name = ParameterValue(disposition, "filename");
                        if (attachment.name.empty()) {
                            attachment.name = ParameterValue(part_headers.count("content-type")
                                                                 ? part_headers.at("content-type")
                                                                 : "",
                                                             "name");
                        }
                        attachment.content_type =
                            part_headers.count("content-type") ? part_headers.at("content-type")
                                                               : "application/octet-stream";
                        attachment.size = decoded_body.size();
                        attachment.content_id =
                            StripAngleBrackets(part_headers.count("content-id") ? part_headers.at("content-id") : "");
                        attachment.disposition = DispositionToken(disposition);
                        attachment.download_complete = true;
                        result.record.attachments.push_back(std::move(attachment));
                    }
                    result.embedded_attachment_payloads.push_back(decoded_body);
                    ++attachment_index;
                    continue;
                }
                if (part_type.find("multipart/alternative") != std::string::npos) {
                    const std::string nested_boundary =
                        ParameterValue(part_headers.at("content-type"), "boundary");
                    if (!nested_boundary.empty()) {
                        const std::string nested_delimiter = "--" + nested_boundary;
                        std::size_t nested_offset = 0;
                        while (true) {
                            const std::size_t nested_begin = decoded_body.find(nested_delimiter, nested_offset);
                            if (nested_begin == std::string::npos) {
                                break;
                            }
                            std::size_t nested_content_begin = nested_begin + nested_delimiter.size();
                            if (nested_content_begin + 1 < decoded_body.size() &&
                                decoded_body[nested_content_begin] == '-' &&
                                decoded_body[nested_content_begin + 1] == '-') {
                                break;
                            }
                            if (nested_content_begin < decoded_body.size() &&
                                decoded_body[nested_content_begin] == '\r') {
                                ++nested_content_begin;
                            }
                            if (nested_content_begin < decoded_body.size() &&
                                decoded_body[nested_content_begin] == '\n') {
                                ++nested_content_begin;
                            }
                            const std::size_t nested_next =
                                decoded_body.find(nested_delimiter, nested_content_begin);
                            const std::string nested_part = decoded_body.substr(
                                nested_content_begin,
                                nested_next == std::string::npos ? std::string::npos
                                                                 : nested_next - nested_content_begin);
                            nested_offset = nested_next == std::string::npos ? decoded_body.size() : nested_next;
                            const auto [nested_headers, nested_body] = ParsePart(nested_part);
                            const std::string nested_type =
                                ToLower(nested_headers.count("content-type") ? nested_headers.at("content-type")
                                                                             : "text/plain");
                            const std::string decoded_nested =
                                DecodeTransferEncodedBody(nested_headers, nested_body);
                            if (nested_type.find("text/html") != std::string::npos) {
                                result.record.html_body = decoded_nested;
                                if (!has_explicit_styled_source) {
                                    result.record.styled_source = StyledDocumentSource::kHtml;
                                }
                            } else if (nested_type.find("text/rtf") != std::string::npos ||
                                       nested_type.find("application/rtf") != std::string::npos) {
                                result.record.rtf_body = decoded_nested;
                                if (!has_explicit_styled_source) {
                                    result.record.styled_source = StyledDocumentSource::kRtf;
                                }
                            } else if (IsHemeraPaigeMimeType(nested_type)) {
                                result.record.paige_native_body = decoded_nested;
                                if (!has_explicit_styled_source) {
                                    result.record.styled_source = StyledDocumentSource::kPaigeNative;
                                }
                            } else {
                                result.record.plain_text_body = decoded_nested;
                            }
                        }
                    }
                    continue;
                }
                if (part_type.find("text/html") != std::string::npos) {
                    result.record.html_body = decoded_body;
                    if (!has_explicit_styled_source) {
                        result.record.styled_source = StyledDocumentSource::kHtml;
                    }
                } else if (part_type.find("text/rtf") != std::string::npos ||
                           part_type.find("application/rtf") != std::string::npos) {
                    result.record.rtf_body = decoded_body;
                    if (!has_explicit_styled_source) {
                        result.record.styled_source = StyledDocumentSource::kRtf;
                    }
                } else if (IsHemeraPaigeMimeType(part_type)) {
                    result.record.paige_native_body = decoded_body;
                    if (!has_explicit_styled_source) {
                        result.record.styled_source = StyledDocumentSource::kPaigeNative;
                    }
                } else if (part_type.find("text/plain") != std::string::npos) {
                    result.record.plain_text_body = decoded_body;
                }
            }
        }
    }
    if (result.record.plain_text_body.empty() && !result.record.rtf_body.empty()) {
        result.record.plain_text_body = StripRtf(result.record.rtf_body);
    }
    if (result.record.plain_text_body.empty() && !result.record.html_body.empty()) {
        result.record.plain_text_body = StripHtml(result.record.html_body);
    }
    result.record.plain_text_body = TrimTrailingLineEndings(std::move(result.record.plain_text_body));
    result.record.html_body = TrimTrailingLineEndings(std::move(result.record.html_body));
    result.record.rtf_body = TrimTrailingLineEndings(std::move(result.record.rtf_body));
    result.record.styled_fidelity = ClassifyStyledDocument(
        RichTextDocument{result.record.plain_text_body,
                         result.record.html_body,
                         false,
                         result.record.rtf_body,
                         result.record.paige_native_body,
                         result.record.styled_source,
                         result.record.styled_fidelity});
    *parsed = std::move(result);
    return true;
}

bool ScanMbxFile(const std::filesystem::path& mbx_path,
                 const std::function<bool(std::int32_t, const std::string&)>& consumer,
                 std::string* error_message) {
    std::ifstream input(mbx_path, std::ios::binary);
    if (!input.is_open()) {
        if (error_message) {
            *error_message = "Unable to read MBX " + mbx_path.string();
        }
        return false;
    }

    std::string contents{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::size_t offset = 0;
    while (offset < contents.size()) {
        std::size_t start = contents.find(kEnvelopePrefix, offset);
        if (start == std::string::npos) {
            break;
        }
        const std::size_t next = contents.find("\n" + std::string(kEnvelopePrefix), start + 1);
        const std::size_t message_end = next == std::string::npos ? contents.size() : next + 1;
        const std::string message = contents.substr(start, message_end - start);
        if (!consumer(static_cast<std::int32_t>(start), message)) {
            return false;
        }
        offset = message_end;
    }
    return true;
}

std::optional<TocData> RebuildTocFromMbx(const ResolvedMailbox& mailbox, std::string* error_message) {
    TocData toc;
    toc.name = mailbox.record.display_name;
    toc.type = mailbox.record.protocol == MailboxProtocol::kImap ? LegacyMailboxType::kImapMailbox
                                                                 : LocalMailboxType(mailbox.record);
    toc.hide_deleted_imap = mailbox.record.protocol == MailboxProtocol::kImap;
    std::int32_t max_unique_id = 0;
    if (std::filesystem::exists(mailbox.mbx_path)) {
        bool okay = ScanMbxFile(
            mailbox.mbx_path,
            [&](std::int32_t offset, const std::string& raw_message) {
                std::string trimmed = raw_message;
                const std::size_t first_newline = trimmed.find('\n');
                if (first_newline == std::string::npos) {
                    return true;
                }
                std::string payload = trimmed.substr(first_newline + 1);
                ParsedStoredMessage parsed;
                if (!ParseStoredMessage(payload, mailbox.record.id, &parsed)) {
                    return true;
                }
                std::uint32_t unique_id = 0;
                if (!parsed.record.id.empty() && IsDigitsOnly(parsed.record.id)) {
                    unique_id = static_cast<std::uint32_t>(std::stoul(parsed.record.id));
                }
                if (unique_id == 0) {
                    unique_id = static_cast<std::uint32_t>(max_unique_id + 1);
                }
                max_unique_id = std::max<std::int32_t>(max_unique_id, static_cast<std::int32_t>(unique_id));
                parsed.record.id = std::to_string(unique_id);
                if (parsed.record.created_at == 0) {
                    parsed.record.created_at = NowUnixSeconds();
                }
                if (parsed.record.updated_at == 0) {
                    parsed.record.updated_at = parsed.record.created_at;
                }
                toc.summaries.push_back(BuildSummaryFromMessage(parsed.record,
                                                                unique_id,
                                                                offset,
                                                                static_cast<std::int32_t>(raw_message.size())));
                return true;
            },
            error_message);
        if (!okay) {
            return std::nullopt;
        }
    }
    toc.next_unique_message_id = max_unique_id;
    toc.unread_status = ComputeUnreadStatus(toc.summaries);
    std::error_code status_error;
    if (std::filesystem::exists(mailbox.mbx_path, status_error)) {
        toc.stored_mbx_size_plus_one = static_cast<std::uint32_t>(std::filesystem::file_size(mailbox.mbx_path)) + 1;
    }
    if (!WriteToc(mailbox.toc_path, toc, error_message)) {
        return std::nullopt;
    }
    return toc;
}

std::optional<TocData> LoadOrRebuildToc(const ResolvedMailbox& mailbox, std::string* error_message) {
    TocData toc;
    const bool toc_exists = std::filesystem::exists(mailbox.toc_path);
    const bool mbx_exists = std::filesystem::exists(mailbox.mbx_path);
    bool needs_rebuild = !toc_exists;
    if (!needs_rebuild && toc_exists) {
        std::string load_error;
        if (!LoadToc(mailbox.toc_path, &toc, &load_error)) {
            needs_rebuild = true;
        } else if (mbx_exists) {
            std::error_code ignored;
            const std::uint32_t actual_size = static_cast<std::uint32_t>(std::filesystem::file_size(mailbox.mbx_path));
            if (toc.stored_mbx_size_plus_one != 0 && toc.stored_mbx_size_plus_one - 1 != actual_size) {
                needs_rebuild = true;
            }
        }
    }
    if (needs_rebuild) {
        return RebuildTocFromMbx(mailbox, error_message);
    }
    return toc;
}

std::vector<StoredMessageEntry> LoadMessageEntries(const std::filesystem::path& root_directory,
                                                   const ResolvedMailbox& mailbox,
                                                   std::string* error_message) {
    std::vector<StoredMessageEntry> entries;
    const auto toc = LoadOrRebuildToc(mailbox, error_message);
    if (!toc) {
        return entries;
    }
    if (!std::filesystem::exists(mailbox.mbx_path)) {
        return entries;
    }

    std::ifstream input(mailbox.mbx_path, std::ios::binary);
    if (!input.is_open()) {
        if (error_message) {
            *error_message = "Unable to read mailbox " + mailbox.mbx_path.string();
        }
        return {};
    }
    std::string contents{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    entries.reserve(toc->summaries.size());
    for (const auto& summary : toc->summaries) {
        if (summary.offset < 0 || summary.length <= 0 ||
            static_cast<std::size_t>(summary.offset + summary.length) > contents.size()) {
            continue;
        }
        std::string raw_message =
            contents.substr(static_cast<std::size_t>(summary.offset), static_cast<std::size_t>(summary.length));
        const std::size_t first_newline = raw_message.find('\n');
        if (first_newline == std::string::npos) {
            continue;
        }
        raw_message = raw_message.substr(first_newline + 1);
        ParsedStoredMessage parsed;
        if (!ParseStoredMessage(raw_message, mailbox.record.id, &parsed)) {
            continue;
        }
        const std::string numeric_id = std::to_string(summary.unique_id);
        parsed.record.id = parsed.logical_id.empty() ? numeric_id : parsed.logical_id;
        parsed.record.mailbox_id = mailbox.record.id;
        if (parsed.record.created_at == 0) {
            parsed.record.created_at = summary.arrival_seconds != 0 ? summary.arrival_seconds : summary.seconds;
        }
        if (parsed.record.updated_at == 0) {
            parsed.record.updated_at = summary.seconds;
        }
        if (!parsed.has_explicit_legacy_status) {
            parsed.record.legacy_status = LegacyStatusFromSummaryState(summary.state);
            parsed.record.unread = parsed.record.legacy_status == LegacyMessageStatus::kUnread;
        }
        if (!parsed.has_explicit_label) {
            parsed.record.label_index = summary.label;
        }
        if (!parsed.has_explicit_junk_score) {
            parsed.record.junk_score = summary.junk_score;
        }
        if (!parsed.has_explicit_manually_junked) {
            parsed.record.manually_junked = summary.manually_junked;
        }
        if (parsed.record.sender.empty()) {
            parsed.record.sender = FixedStringView(summary.from.data(), summary.from.size());
        }
        if (parsed.record.subject.empty()) {
            parsed.record.subject = FixedStringView(summary.subject.data(), summary.subject.size());
        }
        for (std::size_t index = 0; index < parsed.record.attachments.size(); ++index) {
            if (parsed.record.attachments[index].payload_path.empty()) {
                if (const auto path = ExistingAttachmentPath(root_directory,
                                                             static_cast<std::uint32_t>(summary.unique_id),
                                                             index)) {
                    parsed.record.attachments[index].payload_path = path->string();
                }
            }
        }
        entries.push_back({summary, std::move(parsed)});
    }
    return entries;
}

std::optional<ResolvedMailbox> ResolveLocalMailboxFromPath(const std::filesystem::path& scope_directory,
                                                           const std::filesystem::path& entry_path,
                                                           std::string parent_id) {
    MailboxRecord record;
    record.path = entry_path;
    record.parent_id = std::move(parent_id);
    record.account_id.clear();
    record.protocol = MailboxProtocol::kLocal;
    record.is_remote = false;
    record.remote_name.clear();
    record.kind = entry_path.extension() == ".fol" ? MailboxKind::kFolder : MailboxKind::kMailbox;
    const std::string stem = entry_path.stem().string();
    if (const auto system_id = SystemMailboxIdFromStem(stem)) {
        record.id = *system_id;
        record.system_mailbox = (*system_id == "inbox" || *system_id == "out" || *system_id == "trash" ||
                                 *system_id == "junk");
        record.display_name = *system_id == "inbox" ? "Inbox"
                             : *system_id == "out"  ? "Out"
                             : *system_id == "trash" ? "Trash"
                             : *system_id == "junk"  ? "Junk"
                             : *system_id == "sent"  ? "Sent"
                                                      : "Drafts";
    } else {
        record.id = stem;
        record.display_name = stem;
        record.system_mailbox = false;
    }

    if (record.kind == MailboxKind::kMailbox) {
        const std::filesystem::path toc_path = entry_path.parent_path() / (stem + ".toc");
        std::string ignored;
        if (std::filesystem::exists(toc_path)) {
            TocData toc;
            if (LoadToc(toc_path, &toc, &ignored)) {
                if (!toc.name.empty()) {
                    record.display_name = toc.name;
                }
                record.message_count = toc.summaries.size();
            }
        }
        ResolvedMailbox resolved;
        resolved.record = std::move(record);
        resolved.scope_directory = scope_directory;
        resolved.mbx_path = entry_path;
        resolved.toc_path = entry_path.parent_path() / (stem + ".toc");
        return resolved;
    }

    record.message_count = 0;
    ResolvedMailbox resolved;
    resolved.record = std::move(record);
    resolved.scope_directory = scope_directory;
    return resolved;
}

void GatherLocalMailboxes(const std::filesystem::path& scope_directory,
                          std::string parent_id,
                          std::vector<ResolvedMailbox>* out) {
    if (!std::filesystem::exists(scope_directory)) {
        return;
    }

    std::set<std::string> descmap_names;
    const auto descmap_path = LocalDescmapPath(scope_directory);
    if (std::filesystem::exists(descmap_path)) {
        std::ifstream input(descmap_path);
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty()) {
                continue;
            }
            const std::size_t first = line.find(',');
            const std::size_t second = line.find(',', first == std::string::npos ? first : first + 1);
            const std::size_t third = line.find(',', second == std::string::npos ? second : second + 1);
            if (first == std::string::npos || second == std::string::npos) {
                continue;
            }
            const std::string name = line.substr(0, first);
            const std::string filename = line.substr(first + 1, second - first - 1);
            const char type_code = second + 1 < line.size() ? line[second + 1] : 'M';
            if (!LocalMailboxTypeFromDescmapCode(type_code, filename)) {
                continue;
            }
            const std::filesystem::path child_path = scope_directory / filename;
            descmap_names.insert(child_path.filename().string());
            if (!std::filesystem::exists(child_path)) {
                continue;
            }
            if (const auto resolved = ResolveLocalMailboxFromPath(scope_directory, child_path, parent_id)) {
                ResolvedMailbox mailbox = *resolved;
                if (!name.empty()) {
                    mailbox.record.display_name = name;
                }
                out->push_back(mailbox);
                if (mailbox.record.kind == MailboxKind::kFolder) {
                    GatherLocalMailboxes(child_path, mailbox.record.id, out);
                }
            }
        }
    }

    for (const auto& entry : std::filesystem::directory_iterator(scope_directory)) {
        if (!entry.is_regular_file() && !entry.is_directory()) {
            continue;
        }
        const std::filesystem::path path = entry.path();
        if (path.filename() == kDescmapName || path.filename() == kImapDirectoryName || path.filename() == kAttachDirectoryName ||
            path.filename() == kDraftSidecarDirectoryName || path.filename() == kLegacyMailboxesDirectoryName ||
            path.filename() == kLegacyDraftsDirectoryName || path.filename() == kLegacyAttachmentsDirectoryName) {
            continue;
        }
        if (path.extension() != ".mbx" && path.extension() != ".fol") {
            continue;
        }
        if (descmap_names.count(path.filename().string()) != 0) {
            continue;
        }
        if (const auto resolved = ResolveLocalMailboxFromPath(scope_directory, path, parent_id)) {
            out->push_back(*resolved);
            if (resolved->record.kind == MailboxKind::kFolder) {
                GatherLocalMailboxes(path, resolved->record.id, out);
            }
        }
    }
}

std::vector<ResolvedMailbox> ListLocalResolvedMailboxes(const std::filesystem::path& root_directory) {
    std::vector<ResolvedMailbox> mailboxes;
    GatherLocalMailboxes(root_directory, {}, &mailboxes);
    return mailboxes;
}

std::vector<ResolvedMailbox> ListImapResolvedMailboxes(const std::filesystem::path& root_directory) {
    std::vector<ResolvedMailbox> mailboxes;
    const auto imap_root = ImapRoot(root_directory);
    if (!std::filesystem::exists(imap_root)) {
        return mailboxes;
    }
    for (const auto& account_entry : std::filesystem::directory_iterator(imap_root)) {
        if (!account_entry.is_directory()) {
            continue;
        }
        const std::filesystem::path account_root = account_entry.path();
        const std::filesystem::path list_path = ImapMboxListPath(account_root);
        if (!std::filesystem::exists(list_path)) {
            continue;
        }
        std::ifstream input(list_path);
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty()) {
                continue;
            }
            std::string working = line;
            std::vector<std::string> fields;
            std::string current;
            for (std::size_t index = 0; index < working.size(); ++index) {
                const char ch = working[index];
                if (ch == ',' && index + 1 < working.size() && working[index + 1] == ',') {
                    current.push_back(',');
                    ++index;
                    continue;
                }
                if (ch == ',') {
                    fields.push_back(current);
                    current.clear();
                    continue;
                }
                current.push_back(ch);
            }
            fields.push_back(current);
            if (fields.size() < 2) {
                continue;
            }
            const std::string remote_name = UnescapeImapName(fields[0]);
            const std::string directory_name = fields[1];
            const std::string account_id = account_entry.path().filename().string();
            ResolvedMailbox resolved;
            resolved.record.id = account_id + ":" + SanitizePathComponent(remote_name);
            resolved.record.display_name = remote_name;
            resolved.record.path = account_root / directory_name / (directory_name + ".mbx");
            resolved.record.account_id = account_id;
            resolved.record.protocol = MailboxProtocol::kImap;
            resolved.record.remote_name = remote_name;
            resolved.record.is_remote = true;
            resolved.record.system_mailbox = false;
            resolved.record.kind = MailboxKind::kMailbox;
            const std::size_t split = remote_name.find_last_of("/.");
            if (split != std::string::npos && split > 0) {
                resolved.record.parent_id = account_id + ":" + SanitizePathComponent(remote_name.substr(0, split));
            }
            resolved.scope_directory = account_root / directory_name;
            resolved.imap_account_root = account_root;
            resolved.imap_directory_name = directory_name;
            resolved.mbx_path = resolved.scope_directory / (directory_name + ".mbx");
            resolved.toc_path = resolved.scope_directory / (directory_name + ".toc");
            std::string ignored;
            if (std::filesystem::exists(resolved.toc_path)) {
                TocData toc;
                if (LoadToc(resolved.toc_path, &toc, &ignored)) {
                    resolved.record.message_count = toc.summaries.size();
                    if (!toc.name.empty()) {
                        resolved.record.display_name = toc.name;
                    }
                }
            }
            mailboxes.push_back(std::move(resolved));
        }
    }
    return mailboxes;
}

std::vector<ResolvedMailbox> ListAllResolvedMailboxes(const std::filesystem::path& root_directory) {
    auto local = ListLocalResolvedMailboxes(root_directory);
    auto imap = ListImapResolvedMailboxes(root_directory);
    local.insert(local.end(),
                 std::make_move_iterator(imap.begin()),
                 std::make_move_iterator(imap.end()));
    std::sort(local.begin(), local.end(), [](const ResolvedMailbox& left, const ResolvedMailbox& right) {
        if (left.record.parent_id != right.record.parent_id) {
            return left.record.parent_id < right.record.parent_id;
        }
        return left.record.display_name < right.record.display_name;
    });
    return local;
}

std::optional<ResolvedMailbox> ResolveMailboxInternal(const std::filesystem::path& root_directory,
                                                      std::string_view mailbox_id) {
    const auto all = ListAllResolvedMailboxes(root_directory);
    for (const auto& mailbox : all) {
        if (mailbox.record.id == mailbox_id) {
            return mailbox;
        }
    }
    return std::nullopt;
}

bool WriteDescmapForScope(const std::filesystem::path& scope_directory, std::string* error_message) {
    std::vector<ResolvedMailbox> children;
    if (!std::filesystem::exists(scope_directory)) {
        return true;
    }
    for (const auto& entry : std::filesystem::directory_iterator(scope_directory)) {
        if (!entry.is_regular_file() && !entry.is_directory()) {
            continue;
        }
        const std::filesystem::path path = entry.path();
        if (path.extension() != ".mbx" && path.extension() != ".fol") {
            continue;
        }
        if (const auto resolved = ResolveLocalMailboxFromPath(scope_directory, path, {})) {
            children.push_back(*resolved);
        }
    }
    std::sort(children.begin(), children.end(), [](const ResolvedMailbox& left, const ResolvedMailbox& right) {
        return left.record.display_name < right.record.display_name;
    });

    std::ostringstream output;
    for (const auto& child : children) {
        const LegacyMailboxType type = child.record.kind == MailboxKind::kFolder
                                           ? LegacyMailboxType::kFolder
                                           : LocalMailboxType(child.record);
        std::string unread_status = "N";
        std::string ignored;
        if (child.record.kind == MailboxKind::kMailbox && std::filesystem::exists(child.toc_path)) {
            TocData toc;
            if (LoadToc(child.toc_path, &toc, &ignored)) {
                unread_status = std::string(1, UnreadStatusCode(toc.unread_status));
            }
        } else {
            unread_status = "U";
        }
        output << child.record.display_name << "," << child.record.path.filename().string() << ","
               << DescmapTypeCode(type) << "," << unread_status << "\n";
    }
    return WriteFile(LocalDescmapPath(scope_directory), output.str(), error_message);
}

bool RewriteAllLocalDescmaps(const std::filesystem::path& root_directory, std::string* error_message) {
    const auto local = ListLocalResolvedMailboxes(root_directory);
    std::set<std::filesystem::path> scopes;
    scopes.insert(root_directory);
    for (const auto& mailbox : local) {
        if (mailbox.record.kind == MailboxKind::kFolder) {
            scopes.insert(mailbox.record.path);
        }
    }
    for (const auto& scope : scopes) {
        if (!WriteDescmapForScope(scope, error_message)) {
            return false;
        }
    }
    return true;
}

bool WriteImapMailboxList(const std::filesystem::path& account_root,
                          const std::vector<ResolvedMailbox>& mailboxes,
                          std::string* error_message) {
    std::ostringstream output;
    for (const auto& mailbox : mailboxes) {
        if (mailbox.record.protocol != MailboxProtocol::kImap) {
            continue;
        }
        output << EscapeImapName(mailbox.record.remote_name) << "," << mailbox.imap_directory_name
               << ",M, ,0,0,0,0,U\n";
    }
    return WriteFile(ImapMboxListPath(account_root), output.str(), error_message);
}

bool EnsureRootDirectories(const std::filesystem::path& root_directory, std::string* error_message) {
    std::error_code create_error;
    std::filesystem::create_directories(root_directory, create_error);
    std::filesystem::create_directories(AttachRoot(root_directory), create_error);
    std::filesystem::create_directories(ImapRoot(root_directory), create_error);
    std::filesystem::create_directories(DraftSidecarRoot(root_directory), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to initialize Eudora store directories: " + create_error.message();
        }
        return false;
    }
    return true;
}

bool EnsureEmptyMailboxFiles(const std::filesystem::path& mbx_path,
                             const std::filesystem::path& toc_path,
                             const MailboxRecord& mailbox,
                             std::string* error_message) {
    std::error_code create_error;
    std::filesystem::create_directories(mbx_path.parent_path(), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create mailbox directory: " + create_error.message();
        }
        return false;
    }
    if (!std::filesystem::exists(mbx_path)) {
        std::ofstream output(mbx_path, std::ios::binary);
        if (!output.is_open()) {
            if (error_message) {
                *error_message = "Unable to create mailbox file: " + mbx_path.string();
            }
            return false;
        }
    }
    if (std::filesystem::exists(toc_path)) {
        return true;
    }
    TocData toc;
    toc.name = DisplayNameForMailbox(mailbox);
    toc.type = mailbox.protocol == MailboxProtocol::kImap ? LegacyMailboxType::kImapMailbox
                                                          : LocalMailboxType(mailbox);
    toc.hide_deleted_imap = mailbox.protocol == MailboxProtocol::kImap;
    toc.unread_status = UnreadStatus::kNo;
    if (std::filesystem::exists(mbx_path)) {
        toc.stored_mbx_size_plus_one = static_cast<std::uint32_t>(std::filesystem::file_size(mbx_path)) + 1;
    }
    return WriteToc(toc_path, toc, error_message);
}

bool MaybeMigrateLegacyStore(const std::filesystem::path& root_directory, std::string* error_message) {
    const bool canonical_exists = std::filesystem::exists(LocalDescmapPath(root_directory)) ||
                                  std::filesystem::exists(root_directory / "In.mbx") ||
                                  std::filesystem::exists(root_directory / "Drafts.mbx");
    if (canonical_exists) {
        return true;
    }

    const bool legacy_exists = std::filesystem::exists(LegacyMailboxesPath(root_directory)) ||
                               std::filesystem::exists(LegacyDraftsPath(root_directory));
    if (!legacy_exists) {
        return true;
    }

    std::vector<MailboxRecord> legacy_mailboxes;
    if (std::filesystem::exists(LegacyMailboxesPath(root_directory))) {
        for (const auto& entry : std::filesystem::directory_iterator(LegacyMailboxesPath(root_directory))) {
            if (!entry.is_directory()) {
                continue;
            }
            const auto metadata_path = entry.path() / "mailbox.ini";
            IniSettingsStore metadata;
            std::string ignored;
            metadata.LoadFromFile(metadata_path, &ignored);
            MailboxRecord mailbox;
            mailbox.id = metadata.GetString("Mailbox", "Id").value_or(entry.path().filename().string());
            mailbox.display_name = metadata.GetString("Mailbox", "DisplayName").value_or(mailbox.id);
            mailbox.account_id = metadata.GetString("Mailbox", "AccountId").value_or("");
            const std::string protocol = ToLower(metadata.GetString("Mailbox", "Protocol").value_or("local"));
            mailbox.protocol = protocol == "imap"  ? MailboxProtocol::kImap
                               : protocol == "pop" ? MailboxProtocol::kPop
                                                   : MailboxProtocol::kLocal;
            mailbox.remote_name = metadata.GetString("Mailbox", "RemoteName").value_or("");
            mailbox.is_remote = metadata.GetBool("Mailbox", "IsRemote", false);
            mailbox.system_mailbox = metadata.GetBool("Mailbox", "SystemMailbox", false);
            legacy_mailboxes.push_back(std::move(mailbox));
        }
    }

    for (const auto& mailbox : legacy_mailboxes) {
        if (!EnsureMailbox(root_directory, mailbox, error_message)) {
            return false;
        }
        for (const auto& entry :
             std::filesystem::directory_iterator(LegacyMailboxesPath(root_directory) / mailbox.id)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".eml") {
                continue;
            }
            MessageRecord message;
            if (!LegacyReadMessageFile(entry.path(), mailbox.id, &message)) {
                continue;
            }
            if (message.mailbox_id.empty()) {
                message.mailbox_id = mailbox.id;
            }
            if (!SaveMessage(root_directory, message, error_message)) {
                return false;
            }
        }
    }

    if (std::filesystem::exists(LegacyDraftsPath(root_directory))) {
        for (const auto& entry : std::filesystem::directory_iterator(LegacyDraftsPath(root_directory))) {
            if (!entry.is_directory()) {
                continue;
            }
            IniSettingsStore metadata;
            std::string ignored;
            if (!metadata.LoadFromFile(entry.path() / "draft.ini", &ignored)) {
                continue;
            }
            ComposeMessage draft;
            draft.id = metadata.GetString("Draft", "Id").value_or(entry.path().filename().string());
            draft.stationery_name = metadata.GetString("Draft", "StationeryName").value_or("");
            draft.signature_name = metadata.GetString("Draft", "SignatureName").value_or("");
            draft.managed_signature.attached =
                metadata.GetBool("Draft", "ManagedSignatureAttached", false);
            draft.managed_signature.name =
                metadata.GetString("Draft", "ManagedSignatureName").value_or("");
            draft.managed_signature.start =
                static_cast<std::size_t>(metadata.GetInt("Draft", "ManagedSignatureStart", 0));
            draft.managed_signature.length =
                static_cast<std::size_t>(metadata.GetInt("Draft", "ManagedSignatureLength", 0));
            draft.managed_signature.plain_text =
                metadata.GetString("Draft", "ManagedSignaturePlainText").value_or("");
            draft.headers.to = metadata.GetString("Headers", "To").value_or("");
            draft.headers.cc = metadata.GetString("Headers", "Cc").value_or("");
            draft.headers.bcc = metadata.GetString("Headers", "Bcc").value_or("");
            draft.headers.subject = metadata.GetString("Headers", "Subject").value_or("");
            draft.headers.from_persona = metadata.GetString("Headers", "FromPersona").value_or("");
            draft.headers.reply_to = metadata.GetString("Headers", "ReplyTo").value_or("");
            draft.policy = ReadPolicy(metadata);
            draft.options = ReadOptions(metadata, draft.policy);
            draft.body.plain_text = ReadFile(entry.path() / "body.txt").value_or("");
            draft.body.html_fragment = ReadFile(entry.path() / "body.html").value_or("");
            draft.body.rtf_fragment = ReadFile(entry.path() / "body.rtf").value_or("");
            draft.body.paige_native_bytes = ReadFile(entry.path() / "body.pg").value_or("");
            draft.body = NormalizeRichTextDocument(draft.body);
            draft.body.read_only = draft.policy.read_only;
            const int attachment_count = metadata.GetInt("Attachments", "Count", 0);
            for (int index = 0; index < attachment_count; ++index) {
                ComposeAttachment attachment;
                const std::string prefix = "Attachment" + std::to_string(index);
                attachment.display_name =
                    metadata.GetString("Attachments", prefix + "DisplayName").value_or("");
                attachment.source_path =
                    metadata.GetString("Attachments", prefix + "SourcePath").value_or("");
                attachment.mime_type =
                    metadata.GetString("Attachments", prefix + "MimeType").value_or("");
                attachment.size = static_cast<std::uint64_t>(
                    std::max(metadata.GetInt("Attachments", prefix + "Size", 0), 0));
                attachment.content_id =
                    metadata.GetString("Attachments", prefix + "ContentId").value_or("");
                attachment.inline_disposition =
                    metadata.GetBool("Attachments", prefix + "InlineDisposition", false);
                draft.attachments.push_back(std::move(attachment));
            }
            if (!SaveDraft(root_directory, draft, error_message)) {
                return false;
            }
        }
    }

    return true;
}

bool EnsureCanonicalStoreReadyInternal(const std::filesystem::path& root_directory, std::string* error_message) {
    if (g_initializing_canonical_store) {
        return true;
    }

    struct ScopedInitializationGuard {
        ScopedInitializationGuard() {
            g_initializing_canonical_store = true;
        }

        ~ScopedInitializationGuard() {
            g_initializing_canonical_store = false;
        }
    } guard;

    if (!EnsureRootDirectories(root_directory, error_message)) {
        return false;
    }
    if (!MaybeMigrateLegacyStore(root_directory, error_message)) {
        return false;
    }
    const std::array<MailboxRecord, 6> defaults = {{
        {"inbox", "Inbox", {}, "", MailboxProtocol::kLocal, "", false, true, 0, MailboxKind::kMailbox, {}},
        {"out", "Out", {}, "", MailboxProtocol::kLocal, "", false, true, 0, MailboxKind::kMailbox, {}},
        {"trash", "Trash", {}, "", MailboxProtocol::kLocal, "", false, true, 0, MailboxKind::kMailbox, {}},
        {"junk", "Junk", {}, "", MailboxProtocol::kLocal, "", false, true, 0, MailboxKind::kMailbox, {}},
        {"sent", "Sent", {}, "", MailboxProtocol::kLocal, "", false, false, 0, MailboxKind::kMailbox, {}},
        {"drafts", "Drafts", {}, "", MailboxProtocol::kLocal, "", false, false, 0, MailboxKind::kMailbox, {}},
    }};
    for (const auto& mailbox : defaults) {
        if (!EnsureMailbox(root_directory, mailbox, error_message)) {
            return false;
        }
    }
    return RewriteAllLocalDescmaps(root_directory, error_message);
}

bool EnsureMailboxInternal(const std::filesystem::path& root_directory,
                           const MailboxRecord& mailbox,
                           std::string* error_message) {
    if (mailbox.id.empty()) {
        if (error_message) {
            *error_message = "Mailbox id must not be empty.";
        }
        return false;
    }
    if (!EnsureCanonicalStoreReadyInternal(root_directory, error_message)) {
        return false;
    }

    if (mailbox.protocol == MailboxProtocol::kImap || mailbox.is_remote) {
        const std::filesystem::path account_root = ImapAccountRoot(root_directory, mailbox.account_id);
        const std::string dirname =
            SanitizePathComponent(mailbox.remote_name.empty() ? mailbox.id : mailbox.remote_name);
        const std::filesystem::path scope_directory = account_root / dirname;
        const std::filesystem::path mbx_path = scope_directory / (dirname + ".mbx");
        const std::filesystem::path toc_path = scope_directory / (dirname + ".toc");
        if (!EnsureEmptyMailboxFiles(mbx_path, toc_path, mailbox, error_message)) {
            return false;
        }
        std::vector<ResolvedMailbox> imap_mailboxes = ListImapResolvedMailboxes(root_directory);
        const std::string expected_id = mailbox.id.empty() ? mailbox.account_id + ":" + dirname : mailbox.id;
        bool found = false;
        for (const auto& existing : imap_mailboxes) {
            if (existing.record.id == expected_id) {
                found = true;
            }
        }
        if (!found) {
            ResolvedMailbox resolved;
            resolved.record = mailbox;
            resolved.record.id = expected_id;
            resolved.record.display_name = DisplayNameForMailbox(mailbox);
            resolved.record.path = mbx_path;
            resolved.record.account_id = mailbox.account_id;
            resolved.record.protocol = MailboxProtocol::kImap;
            resolved.record.remote_name = mailbox.remote_name.empty() ? mailbox.display_name : mailbox.remote_name;
            resolved.record.is_remote = true;
            resolved.scope_directory = scope_directory;
            resolved.imap_account_root = account_root;
            resolved.imap_directory_name = dirname;
            resolved.mbx_path = mbx_path;
            resolved.toc_path = toc_path;
            imap_mailboxes.push_back(std::move(resolved));
        }
        return WriteImapMailboxList(account_root, imap_mailboxes, error_message);
    }

    std::filesystem::path scope_directory = root_directory;
    if (!mailbox.parent_id.empty()) {
        const auto parent = ResolveMailboxInternal(root_directory, mailbox.parent_id);
        if (!parent || parent->record.kind != MailboxKind::kFolder) {
            if (error_message) {
                *error_message = "Parent mailbox folder not found: " + mailbox.parent_id;
            }
            return false;
        }
        scope_directory = parent->record.path;
    }

    if (mailbox.kind == MailboxKind::kFolder) {
        const std::filesystem::path folder_path =
            scope_directory / (SanitizePathComponent(mailbox.id.empty() ? mailbox.display_name : mailbox.id) + ".fol");
        std::error_code create_error;
        std::filesystem::create_directories(folder_path, create_error);
        if (create_error) {
            if (error_message) {
                *error_message = "Unable to create mailbox folder: " + create_error.message();
            }
            return false;
        }
        return RewriteAllLocalDescmaps(root_directory, error_message);
    }

    const std::string stem = LocalRegularMailboxStem(mailbox);
    const std::filesystem::path mbx_path = MailboxMbxPathFromStem(scope_directory, stem);
    const std::filesystem::path toc_path = MailboxTocPathFromStem(scope_directory, stem);
    if (!EnsureEmptyMailboxFiles(mbx_path, toc_path, mailbox, error_message)) {
        return false;
    }
    std::string ignored;
    if (const auto toc = LoadOrRebuildToc({MailboxRecord{mailbox.id,
                                                         DisplayNameForMailbox(mailbox),
                                                         mbx_path,
                                                         mailbox.account_id,
                                                         MailboxProtocol::kLocal,
                                                         "",
                                                         false,
                                                         mailbox.system_mailbox,
                                                         0,
                                                         MailboxKind::kMailbox,
                                                         mailbox.parent_id},
                                          scope_directory,
                                          mbx_path,
                                          toc_path,
                                          {},
                                          {}},
                                         &ignored)) {
        TocData updated = *toc;
        updated.name = DisplayNameForMailbox(mailbox);
        updated.type = LocalMailboxType(mailbox);
        if (!WriteToc(toc_path, updated, error_message)) {
            return false;
        }
    }
    return RewriteAllLocalDescmaps(root_directory, error_message);
}

std::optional<std::pair<ResolvedMailbox, StoredMessageEntry>> ResolveMessageEntry(
    const std::filesystem::path& root_directory,
    std::string_view mailbox_id,
    std::string_view message_id) {
    const auto mailbox = ResolveMailboxInternal(root_directory, mailbox_id);
    if (!mailbox || mailbox->record.kind != MailboxKind::kMailbox) {
        return std::nullopt;
    }
    for (const auto& entry : LoadMessageEntries(root_directory, *mailbox)) {
        if (entry.parsed.record.id == message_id || entry.parsed.logical_id == message_id ||
            std::to_string(entry.summary.unique_id) == message_id) {
            return std::make_pair(*mailbox, entry);
        }
    }
    return std::nullopt;
}

std::vector<std::string> LoadAttachmentPayloadsForMessage(const std::filesystem::path& root_directory,
                                                          std::string_view mailbox_id,
                                                          std::string_view message_id,
                                                          std::uint32_t unique_id,
                                                          const std::vector<MessageAttachment>& attachments) {
    std::vector<std::string> payloads;
    payloads.reserve(attachments.size());
    for (std::size_t index = 0; index < attachments.size(); ++index) {
        std::string payload;
        std::filesystem::path source_path;
        if (!attachments[index].payload_path.empty()) {
            source_path = attachments[index].payload_path;
        } else if (const auto existing = ExistingAttachmentPath(root_directory, unique_id, index)) {
            source_path = *existing;
        } else if (const auto pending =
                       ExistingPendingAttachmentPath(root_directory, mailbox_id, message_id, index)) {
            source_path = *pending;
        }
        if (!source_path.empty()) {
            payload = ReadFile(source_path).value_or("");
        }
        payloads.push_back(std::move(payload));
    }
    return payloads;
}

bool SaveMessageInternal(const std::filesystem::path& root_directory,
                         const MessageRecord& message,
                         std::string* error_message) {
    if (message.mailbox_id.empty()) {
        if (error_message) {
            *error_message = "Message mailbox id must not be empty.";
        }
        return false;
    }
    if (!EnsureCanonicalStoreReadyInternal(root_directory, error_message)) {
        return false;
    }

    auto mailbox = ResolveMailboxInternal(root_directory, message.mailbox_id);
    if (!mailbox) {
        MailboxRecord implicit_mailbox;
        implicit_mailbox.id = message.mailbox_id;
        implicit_mailbox.display_name = message.mailbox_id;
        implicit_mailbox.protocol = MailboxProtocol::kLocal;
        if (!EnsureMailboxInternal(root_directory, implicit_mailbox, error_message)) {
            return false;
        }
        mailbox = ResolveMailboxInternal(root_directory, message.mailbox_id);
    }
    if (!mailbox || mailbox->record.kind != MailboxKind::kMailbox) {
        if (error_message) {
            *error_message = "Mailbox not found: " + std::string(message.mailbox_id);
        }
        return false;
    }

    auto toc = LoadOrRebuildToc(*mailbox, error_message);
    if (!toc) {
        return false;
    }
    auto entries = LoadMessageEntries(root_directory, *mailbox, error_message);

    std::uint32_t unique_id = 0;
    std::string logical_id;
    std::size_t remove_index = static_cast<std::size_t>(-1);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (entries[index].parsed.record.id == message.id ||
            (!message.id.empty() && entries[index].parsed.logical_id == message.id) ||
            (!message.id.empty() && std::to_string(entries[index].summary.unique_id) == message.id)) {
            unique_id = static_cast<std::uint32_t>(entries[index].summary.unique_id);
            logical_id = entries[index].parsed.logical_id;
            remove_index = index;
            break;
        }
    }
    if (unique_id == 0 && !message.id.empty() && IsDigitsOnly(message.id)) {
        unique_id = static_cast<std::uint32_t>(std::stoul(message.id));
    }
    if (logical_id.empty() && !message.id.empty()) {
        logical_id = message.id;
    }
    if (unique_id == 0) {
        unique_id = static_cast<std::uint32_t>(std::max<std::int32_t>(1, toc->next_unique_message_id + 1));
    }
    toc->next_unique_message_id = std::max<std::int32_t>(toc->next_unique_message_id, static_cast<std::int32_t>(unique_id));

    std::vector<MessageAttachment> attachments = message.attachments;
    std::vector<std::string> attachment_payloads = LoadAttachmentPayloadsForMessage(
        root_directory,
        message.mailbox_id,
        logical_id.empty() ? message.id : logical_id,
        unique_id,
        attachments);
    for (std::size_t index = 0; index < attachments.size(); ++index) {
        auto& attachment = attachments[index];
        if (attachment.omitted || !attachment.download_complete) {
            continue;
        }
        const std::string suggested_name = attachment.name.empty() ? "attachment.bin" : attachment.name;
        const auto destination = AttachmentStoragePath(root_directory, unique_id, index, suggested_name);
        const std::filesystem::path source_path =
            attachment.payload_path.empty() ? std::filesystem::path() : std::filesystem::path(attachment.payload_path);
        if (!source_path.empty() && std::filesystem::exists(source_path) && source_path != destination) {
            std::error_code create_error;
            std::filesystem::create_directories(destination.parent_path(), create_error);
            if (create_error) {
                if (error_message) {
                    *error_message = "Unable to create attachment directory: " + create_error.message();
                }
                return false;
            }
            std::error_code copy_error;
            std::filesystem::copy_file(source_path, destination, std::filesystem::copy_options::overwrite_existing, copy_error);
            if (copy_error) {
                if (error_message) {
                    *error_message = "Unable to persist attachment payload: " + copy_error.message();
                }
                return false;
            }
        } else if (!attachment_payloads[index].empty()) {
            if (!WriteFile(destination, attachment_payloads[index], error_message)) {
                return false;
            }
        }
        if (std::filesystem::exists(destination)) {
            attachment.payload_path = destination.string();
            if (attachment_payloads[index].empty()) {
                attachment_payloads[index] = ReadFile(destination).value_or("");
            }
            if (const auto pending = ExistingPendingAttachmentPath(root_directory,
                                                                   message.mailbox_id,
                                                                   logical_id.empty() ? message.id : logical_id,
                                                                   index)) {
                std::error_code ignored;
                std::filesystem::remove(*pending, ignored);
            }
        }
    }

    MessageRecord stored = message;
    stored.id = std::to_string(unique_id);
    stored.attachments = attachments;
    if (stored.created_at == 0) {
        stored.created_at = NowUnixSeconds();
    }
    if (stored.updated_at == 0) {
        stored.updated_at = stored.created_at;
    }
    if (stored.compose_options.word_wrap) {
        stored.plain_text_body = WrapPlainText(stored.plain_text_body, 80);
    }

    const std::string payload = BuildStoredMessageText(stored, unique_id, logical_id, attachment_payloads);
    std::ofstream append(mailbox->mbx_path, std::ios::binary | std::ios::app);
    if (!append.is_open()) {
        if (error_message) {
            *error_message = "Unable to append to mailbox " + mailbox->mbx_path.string();
        }
        return false;
    }
    append.seekp(0, std::ios::end);
    const std::int32_t offset = static_cast<std::int32_t>(append.tellp());
    append.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!append.good()) {
        if (error_message) {
            *error_message = "Unable to append message payload to mailbox.";
        }
        return false;
    }
    append.flush();
    append.close();

    if (remove_index != static_cast<std::size_t>(-1) && remove_index < toc->summaries.size()) {
        toc->summaries.erase(toc->summaries.begin() + static_cast<std::ptrdiff_t>(remove_index));
    }
    toc->summaries.push_back(BuildSummaryFromMessage(stored, unique_id, offset, static_cast<std::int32_t>(payload.size())));
    toc->needs_compact = remove_index != static_cast<std::size_t>(-1);
    toc->unread_status = ComputeUnreadStatus(toc->summaries);
    toc->stored_mbx_size_plus_one = static_cast<std::uint32_t>(std::filesystem::file_size(mailbox->mbx_path)) + 1;
    toc->name = mailbox->record.display_name;
    return WriteToc(mailbox->toc_path, *toc, error_message);
}

bool DeleteMessageInternal(const std::filesystem::path& root_directory,
                           std::string_view mailbox_id,
                           std::string_view message_id,
                           std::string* error_message) {
    if (!EnsureCanonicalStoreReadyInternal(root_directory, error_message)) {
        return false;
    }
    auto mailbox = ResolveMailboxInternal(root_directory, mailbox_id);
    if (!mailbox) {
        if (error_message) {
            *error_message = "Mailbox not found: " + std::string(mailbox_id);
        }
        return false;
    }
    auto toc = LoadOrRebuildToc(*mailbox, error_message);
    if (!toc) {
        return false;
    }
    auto entries = LoadMessageEntries(root_directory, *mailbox, error_message);
    std::size_t remove_index = static_cast<std::size_t>(-1);
    std::uint32_t unique_id = 0;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (entries[index].parsed.record.id == message_id || entries[index].parsed.logical_id == message_id ||
            std::to_string(entries[index].summary.unique_id) == message_id) {
            remove_index = index;
            unique_id = static_cast<std::uint32_t>(entries[index].summary.unique_id);
            break;
        }
    }
    if (remove_index == static_cast<std::size_t>(-1)) {
        return true;
    }
    toc->summaries.erase(toc->summaries.begin() + static_cast<std::ptrdiff_t>(remove_index));
    toc->needs_compact = true;
    toc->unread_status = ComputeUnreadStatus(toc->summaries);
    toc->stored_mbx_size_plus_one = static_cast<std::uint32_t>(std::filesystem::file_size(mailbox->mbx_path)) + 1;
    for (std::size_t index = 0;; ++index) {
        const auto attachment = ExistingAttachmentPath(root_directory, unique_id, index);
        if (!attachment) {
            break;
        }
        std::error_code ignored;
        std::filesystem::remove(*attachment, ignored);
    }
    return WriteToc(mailbox->toc_path, *toc, error_message);
}

}  // namespace

bool EnsureCanonicalStoreReady(const std::filesystem::path& root_directory, std::string* error_message) {
    return EnsureCanonicalStoreReadyInternal(root_directory, error_message);
}

bool EnsureMailbox(const std::filesystem::path& root_directory,
                   const MailboxRecord& mailbox,
                   std::string* error_message) {
    return EnsureMailboxInternal(root_directory, mailbox, error_message);
}

std::vector<MailboxRecord> ListMailboxes(const std::filesystem::path& root_directory) {
    std::string ignored;
    if (!EnsureCanonicalStoreReadyInternal(root_directory, &ignored)) {
        return {};
    }
    std::vector<MailboxRecord> records;
    for (auto mailbox : ListAllResolvedMailboxes(root_directory)) {
        records.push_back(std::move(mailbox.record));
    }
    return records;
}

std::optional<MailboxRecord> GetMailbox(const std::filesystem::path& root_directory, std::string_view mailbox_id) {
    std::string ignored;
    if (!EnsureCanonicalStoreReadyInternal(root_directory, &ignored)) {
        return std::nullopt;
    }
    const auto mailbox = ResolveMailboxInternal(root_directory, mailbox_id);
    return mailbox ? std::optional<MailboxRecord>(mailbox->record) : std::nullopt;
}

bool DeleteMailbox(const std::filesystem::path& root_directory,
                   std::string_view mailbox_id,
                   std::string* error_message) {
    std::string ignored;
    if (!EnsureCanonicalStoreReadyInternal(root_directory, &ignored)) {
        if (error_message && error_message->empty()) {
            *error_message = ignored;
        }
        return false;
    }
    const auto mailbox = ResolveMailboxInternal(root_directory, mailbox_id);
    if (!mailbox) {
        return true;
    }
    std::error_code remove_error;
    if (mailbox->record.protocol == MailboxProtocol::kImap) {
        std::filesystem::remove_all(mailbox->scope_directory, remove_error);
        if (remove_error) {
            if (error_message) {
                *error_message = "Unable to delete IMAP mailbox cache: " + remove_error.message();
            }
            return false;
        }
        return WriteImapMailboxList(mailbox->imap_account_root, ListImapResolvedMailboxes(root_directory), error_message);
    }

    if (mailbox->record.kind == MailboxKind::kFolder) {
        std::filesystem::remove_all(mailbox->record.path, remove_error);
    } else {
        std::filesystem::remove(mailbox->mbx_path, remove_error);
        std::filesystem::remove(mailbox->toc_path, remove_error);
    }
    if (remove_error) {
        if (error_message) {
            *error_message = "Unable to delete mailbox: " + remove_error.message();
        }
        return false;
    }
    return RewriteAllLocalDescmaps(root_directory, error_message);
}

bool RenameMailbox(const std::filesystem::path& root_directory,
                   std::string_view mailbox_id,
                   std::string_view new_mailbox_id,
                   std::string_view new_display_name,
                   std::string* error_message) {
    std::string ignored;
    if (!EnsureCanonicalStoreReadyInternal(root_directory, &ignored)) {
        if (error_message && error_message->empty()) {
            *error_message = ignored;
        }
        return false;
    }
    const auto mailbox = ResolveMailboxInternal(root_directory, mailbox_id);
    if (!mailbox) {
        if (error_message) {
            *error_message = "Mailbox not found: " + std::string(mailbox_id);
        }
        return false;
    }

    if (mailbox->record.protocol == MailboxProtocol::kImap) {
        const std::string directory_name =
            SanitizePathComponent(mailbox->record.remote_name.empty() ? std::string(new_mailbox_id)
                                                                      : std::string(new_display_name.empty()
                                                                                        ? mailbox->record.remote_name
                                                                                        : new_display_name));
        const std::filesystem::path target_scope = mailbox->imap_account_root / directory_name;
        std::error_code rename_error;
        if (mailbox->scope_directory != target_scope) {
            std::filesystem::rename(mailbox->scope_directory, target_scope, rename_error);
        }
        const std::filesystem::path current_mbx_path =
            target_scope / mailbox->mbx_path.filename();
        const std::filesystem::path current_toc_path =
            target_scope / mailbox->toc_path.filename();
        const std::filesystem::path target_mbx_path = target_scope / (directory_name + ".mbx");
        const std::filesystem::path target_toc_path = target_scope / (directory_name + ".toc");
        if (!rename_error && current_mbx_path != target_mbx_path && std::filesystem::exists(current_mbx_path)) {
            std::filesystem::rename(current_mbx_path, target_mbx_path, rename_error);
        }
        if (!rename_error && current_toc_path != target_toc_path && std::filesystem::exists(current_toc_path)) {
            std::filesystem::rename(current_toc_path, target_toc_path, rename_error);
        }
        if (rename_error) {
            if (error_message) {
                *error_message = "Unable to rename IMAP mailbox cache: " + rename_error.message();
            }
            return false;
        }
        auto imap_mailboxes = ListImapResolvedMailboxes(root_directory);
        for (auto& entry : imap_mailboxes) {
            if (entry.record.id == mailbox_id) {
                entry.record.id = std::string(new_mailbox_id);
                if (!new_display_name.empty()) {
                    entry.record.display_name = std::string(new_display_name);
                    entry.record.remote_name = std::string(new_display_name);
                }
                entry.scope_directory = target_scope;
                entry.imap_directory_name = directory_name;
                entry.mbx_path = target_scope / (directory_name + ".mbx");
                entry.toc_path = target_scope / (directory_name + ".toc");
                entry.record.path = entry.mbx_path;
            }
        }
        return WriteImapMailboxList(mailbox->imap_account_root, imap_mailboxes, error_message);
    }

    MailboxRecord updated = mailbox->record;
    updated.id = std::string(new_mailbox_id);
    if (!new_display_name.empty()) {
        updated.display_name = std::string(new_display_name);
    }
    const std::filesystem::path scope_directory =
        mailbox->record.parent_id.empty()
            ? root_directory
            : ResolveMailboxInternal(root_directory, mailbox->record.parent_id)->record.path;
    const std::filesystem::path target_path =
        mailbox->record.kind == MailboxKind::kFolder
            ? scope_directory / (SanitizePathComponent(std::string(new_mailbox_id)) + ".fol")
            : MailboxMbxPathFromStem(scope_directory, LocalRegularMailboxStem(updated));
    std::error_code rename_error;
    if (mailbox->record.kind == MailboxKind::kFolder) {
        std::filesystem::rename(mailbox->record.path, target_path, rename_error);
    } else {
        std::filesystem::rename(mailbox->mbx_path, target_path, rename_error);
        if (!rename_error) {
            std::filesystem::rename(mailbox->toc_path, target_path.parent_path() / (target_path.stem().string() + ".toc"), rename_error);
        }
    }
    if (rename_error) {
        if (error_message) {
            *error_message = "Unable to rename mailbox: " + rename_error.message();
        }
        return false;
    }
    if (mailbox->record.kind == MailboxKind::kMailbox) {
        const auto renamed = ResolveMailboxInternal(root_directory, new_mailbox_id);
        if (renamed) {
            std::string toc_error;
            if (auto toc = LoadOrRebuildToc(*renamed, &toc_error)) {
                toc->name = DisplayNameForMailbox(updated);
                if (!WriteToc(renamed->toc_path, *toc, error_message)) {
                    return false;
                }
            }
        }
    }
    return RewriteAllLocalDescmaps(root_directory, error_message);
}

bool SaveMessage(const std::filesystem::path& root_directory,
                 const MessageRecord& message,
                 std::string* error_message) {
    return SaveMessageInternal(root_directory, message, error_message);
}

std::vector<MessageRecord> ListMessages(const std::filesystem::path& root_directory, std::string_view mailbox_id) {
    std::string ignored;
    if (!EnsureCanonicalStoreReadyInternal(root_directory, &ignored)) {
        return {};
    }
    const auto mailbox = ResolveMailboxInternal(root_directory, mailbox_id);
    if (!mailbox) {
        return {};
    }
    std::vector<MessageRecord> messages;
    for (auto& entry : LoadMessageEntries(root_directory, *mailbox, &ignored)) {
        messages.push_back(std::move(entry.parsed.record));
    }
    return messages;
}

std::optional<MessageRecord> GetMessage(const std::filesystem::path& root_directory,
                                        std::string_view mailbox_id,
                                        std::string_view message_id) {
    std::string ignored;
    if (!EnsureCanonicalStoreReadyInternal(root_directory, &ignored)) {
        return std::nullopt;
    }
    const auto resolved = ResolveMessageEntry(root_directory, mailbox_id, message_id);
    if (!resolved) {
        return std::nullopt;
    }
    return resolved->second.parsed.record;
}

bool DeleteMessage(const std::filesystem::path& root_directory,
                   std::string_view mailbox_id,
                   std::string_view message_id,
                   std::string* error_message) {
    return DeleteMessageInternal(root_directory, mailbox_id, message_id, error_message);
}

bool CopyMessage(const std::filesystem::path& root_directory,
                 std::string_view source_mailbox_id,
                 std::string_view message_id,
                 std::string_view destination_mailbox_id,
                 std::string* error_message) {
    const auto source = GetMessage(root_directory, source_mailbox_id, message_id);
    if (!source) {
        if (error_message) {
            *error_message = "Unable to find message to copy.";
        }
        return false;
    }
    MessageRecord copied = *source;
    copied.id = std::string(message_id.empty() ? source->id : message_id);
    copied.mailbox_id = std::string(destination_mailbox_id);
    return SaveMessageInternal(root_directory, copied, error_message);
}

bool MoveMessage(const std::filesystem::path& root_directory,
                 std::string_view source_mailbox_id,
                 std::string_view message_id,
                 std::string_view destination_mailbox_id,
                 std::string* error_message) {
    const auto source = GetMessage(root_directory, source_mailbox_id, message_id);
    if (!source) {
        if (error_message) {
            *error_message = "Unable to find message to move.";
        }
        return false;
    }
    MessageRecord moved = *source;
    moved.id = std::string(message_id.empty() ? source->id : message_id);
    moved.mailbox_id = std::string(destination_mailbox_id);
    if (!SaveMessageInternal(root_directory, moved, error_message)) {
        return false;
    }
    return DeleteMessageInternal(root_directory, source_mailbox_id, message_id, error_message);
}

bool CompactMailbox(const std::filesystem::path& root_directory,
                    std::string_view mailbox_id,
                    std::string* error_message) {
    if (!EnsureCanonicalStoreReadyInternal(root_directory, error_message)) {
        return false;
    }

    const auto mailbox = ResolveMailboxInternal(root_directory, mailbox_id);
    if (!mailbox || mailbox->record.kind != MailboxKind::kMailbox) {
        if (error_message) {
            *error_message = "Mailbox not found: " + std::string(mailbox_id);
        }
        return false;
    }

    auto toc = LoadOrRebuildToc(*mailbox, error_message);
    if (!toc) {
        return false;
    }

    const auto entries = LoadMessageEntries(root_directory, *mailbox, error_message);
    const std::filesystem::path compact_path = mailbox->mbx_path;
    const std::filesystem::path temp_path = compact_path.string() + ".compact";

    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        if (error_message) {
            *error_message = "Unable to open compact mailbox output: " + temp_path.string();
        }
        return false;
    }

    TocData compacted = *toc;
    compacted.summaries.clear();
    compacted.needs_compact = false;
    compacted.unread_status = UnreadStatus::kUnknown;
    compacted.stored_mbx_size_plus_one = 0;

    std::int32_t max_unique_id = compacted.next_unique_message_id;
    std::int32_t offset = 0;
    for (const auto& entry : entries) {
        MessageRecord stored = entry.parsed.record;
        const std::uint32_t unique_id = static_cast<std::uint32_t>(entry.summary.unique_id);
        stored.id = std::to_string(unique_id);
        const std::vector<std::string> attachment_payloads = LoadAttachmentPayloadsForMessage(
            root_directory,
            mailbox_id,
            entry.parsed.logical_id.empty() ? stored.id : entry.parsed.logical_id,
            unique_id,
            stored.attachments);
        const std::string payload =
            BuildStoredMessageText(stored, unique_id, entry.parsed.logical_id, attachment_payloads);
        output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        if (!output.good()) {
            if (error_message) {
                *error_message = "Unable to write compacted mailbox payload.";
            }
            std::error_code ignored;
            std::filesystem::remove(temp_path, ignored);
            return false;
        }
        compacted.summaries.push_back(BuildSummaryFromMessage(
            stored, unique_id, offset, static_cast<std::int32_t>(payload.size())));
        offset += static_cast<std::int32_t>(payload.size());
        max_unique_id = std::max<std::int32_t>(max_unique_id, static_cast<std::int32_t>(unique_id));
    }

    output.flush();
    output.close();

    std::error_code replace_error;
    std::filesystem::rename(temp_path, mailbox->mbx_path, replace_error);
    if (replace_error) {
        std::error_code ignored;
        std::filesystem::remove(mailbox->mbx_path, ignored);
        replace_error.clear();
        std::filesystem::rename(temp_path, mailbox->mbx_path, replace_error);
    }
    if (replace_error) {
        if (error_message) {
            *error_message = "Unable to replace compacted mailbox: " + replace_error.message();
        }
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        return false;
    }

    compacted.next_unique_message_id = max_unique_id;
    compacted.unread_status = ComputeUnreadStatus(compacted.summaries);
    std::error_code size_error;
    compacted.stored_mbx_size_plus_one =
        std::filesystem::exists(mailbox->mbx_path, size_error)
            ? static_cast<std::uint32_t>(std::filesystem::file_size(mailbox->mbx_path)) + 1
            : 0;
    return WriteToc(mailbox->toc_path, compacted, error_message);
}

bool CompactAllMailboxes(const std::filesystem::path& root_directory, std::string* error_message) {
    if (!EnsureCanonicalStoreReadyInternal(root_directory, error_message)) {
        return false;
    }

    bool compacted_any = false;
    for (const auto& mailbox : ListAllResolvedMailboxes(root_directory)) {
        if (mailbox.record.kind != MailboxKind::kMailbox) {
            continue;
        }
        if (mailbox.record.protocol == MailboxProtocol::kImap ||
            mailbox.record.protocol == MailboxProtocol::kSmtp) {
            continue;
        }
        if (!CompactMailbox(root_directory, mailbox.record.id, error_message)) {
            return false;
        }
        compacted_any = true;
    }
    return compacted_any;
}

bool SaveAttachmentPayload(const std::filesystem::path& root_directory,
                           std::string_view mailbox_id,
                           std::string_view message_id,
                           std::size_t attachment_index,
                           std::string_view suggested_name,
                           std::string_view payload,
                           std::string* error_message) {
    std::string ignored;
    if (!EnsureCanonicalStoreReadyInternal(root_directory, &ignored)) {
        if (error_message && error_message->empty()) {
            *error_message = ignored;
        }
        return false;
    }
    const auto resolved = ResolveMessageEntry(root_directory, mailbox_id, message_id);
    if (!resolved) {
        return WriteFile(PendingAttachmentStoragePath(root_directory,
                                                      mailbox_id,
                                                      message_id,
                                                      attachment_index,
                                                      suggested_name),
                         payload,
                         error_message);
    }
    return WriteFile(AttachmentStoragePath(root_directory,
                                           static_cast<std::uint32_t>(resolved->second.summary.unique_id),
                                           attachment_index,
                                           suggested_name),
                     payload,
                     error_message);
}

bool ImportAttachmentFile(const std::filesystem::path& root_directory,
                          std::string_view mailbox_id,
                          std::string_view message_id,
                          std::size_t attachment_index,
                          const std::filesystem::path& source_path,
                          std::string* error_message) {
    const auto payload = ReadFile(source_path);
    if (!payload) {
        if (error_message) {
            *error_message = "Unable to read attachment source file " + source_path.string();
        }
        return false;
    }
    return SaveAttachmentPayload(root_directory,
                                 mailbox_id,
                                 message_id,
                                 attachment_index,
                                 source_path.filename().string(),
                                 *payload,
                                 error_message);
}

std::optional<std::string> LoadAttachmentPayload(const std::filesystem::path& root_directory,
                                                 std::string_view mailbox_id,
                                                 std::string_view message_id,
                                                 std::size_t attachment_index) {
    const auto path = AttachmentPath(root_directory, mailbox_id, message_id, attachment_index);
    return path ? ReadFile(*path) : std::nullopt;
}

std::optional<std::filesystem::path> AttachmentPath(const std::filesystem::path& root_directory,
                                                    std::string_view mailbox_id,
                                                    std::string_view message_id,
                                                    std::size_t attachment_index) {
    std::string ignored;
    if (!EnsureCanonicalStoreReadyInternal(root_directory, &ignored)) {
        return std::nullopt;
    }
    const auto resolved = ResolveMessageEntry(root_directory, mailbox_id, message_id);
    if (!resolved) {
        return std::nullopt;
    }
    return ExistingAttachmentPath(root_directory,
                                  static_cast<std::uint32_t>(resolved->second.summary.unique_id),
                                  attachment_index);
}

bool SaveDraft(const std::filesystem::path& root_directory,
               const ComposeMessage& draft,
               std::string* error_message) {
    MailboxRecord drafts_mailbox;
    drafts_mailbox.id = "drafts";
    drafts_mailbox.display_name = "Drafts";
    drafts_mailbox.protocol = MailboxProtocol::kLocal;
    if (!EnsureMailboxInternal(root_directory, drafts_mailbox, error_message)) {
        return false;
    }
    if (!SaveDraftSidecar(root_directory, draft, error_message)) {
        return false;
    }
    return SaveMessageInternal(root_directory, MessageRecordFromDraft(draft), error_message);
}

std::optional<ComposeMessage> GetDraft(const std::filesystem::path& root_directory, std::string_view draft_id) {
    std::string ignored;
    if (!EnsureCanonicalStoreReadyInternal(root_directory, &ignored)) {
        return std::nullopt;
    }
    const auto resolved = ResolveMessageEntry(root_directory, "drafts", draft_id);
    if (!resolved) {
        return std::nullopt;
    }
    const std::string sidecar_id =
        resolved->second.parsed.logical_id.empty() ? resolved->second.parsed.record.id
                                                   : resolved->second.parsed.logical_id;
    return ComposeMessageFromDraft(root_directory,
                                   resolved->second.parsed.record,
                                   LoadDraftSidecar(root_directory, sidecar_id));
}

std::vector<ComposeMessage> ListDrafts(const std::filesystem::path& root_directory) {
    std::vector<ComposeMessage> drafts;
    std::string ignored;
    if (!EnsureCanonicalStoreReadyInternal(root_directory, &ignored)) {
        return drafts;
    }
    const auto mailbox = ResolveMailboxInternal(root_directory, "drafts");
    if (!mailbox) {
        return drafts;
    }
    const auto entries = LoadMessageEntries(root_directory, *mailbox, &ignored);
    drafts.reserve(entries.size());
    for (const auto& entry : entries) {
        const std::string sidecar_id =
            entry.parsed.logical_id.empty() ? entry.parsed.record.id : entry.parsed.logical_id;
        ComposeMessage draft =
            ComposeMessageFromDraft(root_directory,
                                    entry.parsed.record,
                                    LoadDraftSidecar(root_directory, sidecar_id));
        if (draft.id.empty()) {
            draft.id = sidecar_id;
        }
        drafts.push_back(std::move(draft));
    }
    return drafts;
}

}  // namespace hermes::eudora
