#include "HaikuWazooWindow.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <ctime>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <Alert.h>
#include <Button.h>
#include <Clipboard.h>
#include <CheckBox.h>
#include <Entry.h>
#include <GroupView.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <Message.h>
#include <MessageRunner.h>
#include <MenuField.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageFilter.h>
#include <Messenger.h>
#include <OutlineListView.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <Roster.h>
#include <StringItem.h>
#include <StringView.h>
#include <Tab.h>
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>
#include <private/interface/ColumnListView.h>
#include <private/interface/ColumnTypes.h>

#include "HaikuFindSupport.h"
#include "HaikuShellHost.h"
#include "PaigeEditorView.h"
#include "hermes/AccountService.h"
#include "hermes/DirectoryServiceCatalog.h"
#include "hermes/FilterStore.h"
#include "hermes/MailTaskModel.h"
#include "hermes/MailboxWorkflow.h"
#include "hermes/NicknameStore.h"
#include "hermes/PaigeRichTextSurface.h"
#include "hermes/SignatureStore.h"
#include "hermes/StationeryStore.h"
#include "hermes/WorkspaceModel.h"

namespace hemera::haiku {

namespace {

constexpr uint32_t kPaneSelectionMessage = 'wpsl';
constexpr uint32_t kPaneInvokeMessage = 'wpin';
constexpr uint32_t kPaneSaveMessage = 'wpav';
constexpr uint32_t kPaneDeleteMessage = 'wpdl';
constexpr uint32_t kPaneActionMessage = 'wpac';
constexpr uint32_t kPaneNewMessage = 'wpnw';
constexpr uint32_t kPaneDuplicateGenericMessage = 'wpdp';
constexpr uint32_t kPanePrimaryCommandMessage = 'wppr';
constexpr uint32_t kPaneSecondaryCommandMessage = 'wpsc';
constexpr uint32_t kWazooPrintPreviewMessage = 'wpvp';
constexpr uint32_t kWazooPrintDirectMessage = 'wpvd';
constexpr uint32_t kDirectoryQueryModifiedMessage = 'dqmd';
constexpr uint32_t kPersonaSelectionMessage = 'prsl';
constexpr uint32_t kPersonaNewMessage = 'prnw';
constexpr uint32_t kPersonaSaveMessage = 'prsv';
constexpr uint32_t kPersonaDeleteMessage = 'prdl';
constexpr uint32_t kPersonaAuthorizeMessage = 'prau';
constexpr uint32_t kPersonaReauthorizeMessage = 'prra';
constexpr uint32_t kPersonaForgetTokensMessage = 'prfg';
constexpr uint32_t kPersonaPollMessage = 'prpl';
constexpr uint32_t kNicknameSelectionMessage = 'nksl';
constexpr uint32_t kNicknameSaveMessage = 'nksv';
constexpr uint32_t kNicknameDeleteMessage = 'nkdl';
constexpr uint32_t kNicknameNewMessage = 'nknw';
constexpr uint32_t kNicknameDuplicateMessage = 'nkdp';
constexpr uint32_t kNicknameComposeMessage = 'nkcm';
constexpr uint32_t kSignatureSelectionMessage = 'sgsl';
constexpr uint32_t kSignatureSaveMessage = 'sgsv';
constexpr uint32_t kSignatureDeleteMessage = 'sgdl';
constexpr uint32_t kSignatureNewMessage = 'sgnw';
constexpr uint32_t kSignatureDuplicateMessage = 'sgdp';
constexpr uint32_t kSignatureRevealMessage = 'sgrv';
constexpr uint32_t kStationerySelectionMessage = 'stsl';
constexpr uint32_t kStationerySaveMessage = 'stsv';
constexpr uint32_t kStationeryDeleteMessage = 'stdl';
constexpr uint32_t kStationeryNewMessage = 'stnw';
constexpr uint32_t kStationeryDuplicateMessage = 'stdp';
constexpr uint32_t kStationeryUseMessage = 'stus';
constexpr uint32_t kStationeryReplyMessage = 'strp';
constexpr uint32_t kStationeryReplyAllMessage = 'stra';
constexpr uint32_t kFilterSelectionMessage = 'ftsl';
constexpr uint32_t kFilterSaveMessage = 'ftsv';
constexpr uint32_t kFilterDeleteMessage = 'ftdl';
constexpr uint32_t kFilterNewMessage = 'ftnw';
constexpr uint32_t kFilterMoveUpMessage = 'ftup';
constexpr uint32_t kFilterMoveDownMessage = 'ftdn';
constexpr uint32_t kFilterReportSelectionMessage = 'frsl';
constexpr uint32_t kFilterReportOpenMessage = 'frop';
constexpr uint32_t kFilterReportClearMessage = 'frcl';
constexpr uint32_t kLinkHistorySelectionMessage = 'lhsl';
constexpr uint32_t kLinkHistoryOpenMessage = 'lhop';
constexpr uint32_t kLinkHistoryRemoveMessage = 'lhrm';
constexpr uint32_t kLinkHistoryClearMessage = 'lhcl';
constexpr uint32_t kDirectoryProviderSelectionMessage = 'dpsl';
constexpr uint32_t kDirectoryResultSelectionMessage = 'drsl';
constexpr uint32_t kDirectorySearchMessage = 'drsr';
constexpr uint32_t kDirectoryComposeToMessage = 'drct';
constexpr uint32_t kDirectoryComposeCcMessage = 'drcc';
constexpr uint32_t kDirectoryComposeBccMessage = 'drcb';
constexpr uint32_t kDirectoryNicknameMessage = 'drnk';
constexpr uint32_t kDirectoryCopyMessage = 'drcp';
constexpr uint32_t kDirectoryKeepOnTopMessage = 'drkt';
constexpr uint32_t kFileBrowserSelectionMessage = 'fbsl';
constexpr uint32_t kFileBrowserOpenMessage = 'fbop';
constexpr uint32_t kFileBrowserRevealMessage = 'fbrv';
constexpr uint32_t kFileBrowserRefreshMessage = 'fbrf';
constexpr uint32_t kSearchRunMessage = 'srch';
constexpr uint32_t kSearchSelectionMessage = 'srsl';
constexpr uint32_t kSearchOpenMessage = 'srop';
constexpr uint32_t kSearchClearScopeMessage = 'srcl';
constexpr uint32_t kPluginSelectionMessage = 'pgsl';
constexpr uint32_t kPluginRescanMessage = 'pgsc';
constexpr uint32_t kPluginRevealUserRootMessage = 'pgur';
constexpr uint32_t kPluginRevealAppRootMessage = 'pgar';
constexpr uint32_t kPluginRevealSelectedMessage = 'pgrs';
constexpr uint32_t kMailboxOpenMessage = 'mbop';
constexpr uint32_t kMailboxFindMessagesMessage = 'mbfm';
constexpr uint32_t kMailboxRenameMessage = 'mbrn';
constexpr uint32_t kMailboxRefreshMessage = 'mbrf';
constexpr uint32_t kMailboxResyncMessage = 'mbrs';
constexpr uint32_t kMailboxResyncTreeMessage = 'mbrt';
constexpr uint32_t kMailboxAutoSyncMessage = 'mbas';
constexpr uint32_t kMailboxShowDeletedMessage = 'mbsd';
constexpr uint32_t kMailboxEmptyTrashMessage = 'mbet';
constexpr uint32_t kMailboxTrimJunkMessage = 'mbtj';
constexpr uint32_t kMailboxCreateConfirmedMessage = 'mbcc';
constexpr uint32_t kMailboxRenameConfirmedMessage = 'mbrc';
constexpr uint32_t kWazooFindMessage = 'wzfd';
constexpr uint32_t kWazooFindAgainMessage = 'wzfa';
constexpr uint32_t kWazooFindConfirmedMessage = 'wzfc';
constexpr uint32_t kWazooFindClosedMessage = 'wzfl';
constexpr uint32_t kWazooToggleDetailsMessage = 'wztd';

enum TaskFieldIndex : int32_t {
    kTaskFieldTask = 0,
    kTaskFieldPersona,
    kTaskFieldStatus,
    kTaskFieldDetails,
    kTaskFieldProgress,
};

enum FilterReportFieldIndex : int32_t {
    kFilterReportFieldTimestamp = 0,
    kFilterReportFieldMailbox,
    kFilterReportFieldSender,
    kFilterReportFieldSubject,
    kFilterReportFieldRules,
};

enum LinkHistoryFieldIndex : int32_t {
    kLinkHistoryFieldType = 0,
    kLinkHistoryFieldTitle,
    kLinkHistoryFieldTarget,
    kLinkHistoryFieldSource,
    kLinkHistoryFieldLaunched,
    kLinkHistoryFieldTimestamp,
};

enum DirectoryResultFieldIndex : int32_t {
    kDirectoryResultFieldName = 0,
    kDirectoryResultFieldEmail,
    kDirectoryResultFieldProvider,
};

enum SearchFieldIndex : int32_t {
    kSearchFieldMailbox = 0,
    kSearchFieldSubject,
    kSearchFieldSender,
    kSearchFieldScore,
};

enum PluginFieldIndex : int32_t {
    kPluginFieldName = 0,
    kPluginFieldVersion,
    kPluginFieldSourceRoot,
    kPluginFieldPath,
    kPluginFieldCapabilities,
};

std::string FilterFieldLabel(FilterField field) {
    switch (field) {
        case FilterField::kFrom:
            return "From";
        case FilterField::kTo:
            return "To";
        case FilterField::kSubject:
            return "Subject";
        case FilterField::kBody:
            return "Body";
    }
    return "Subject";
}

FilterField FilterFieldFromLabel(std::string_view label) {
    if (label == "From") {
        return FilterField::kFrom;
    }
    if (label == "To") {
        return FilterField::kTo;
    }
    if (label == "Body") {
        return FilterField::kBody;
    }
    return FilterField::kSubject;
}

std::string FilterOperationLabel(FilterOperation operation) {
    switch (operation) {
        case FilterOperation::kContains:
            return "Contains";
        case FilterOperation::kEquals:
            return "Equals";
        case FilterOperation::kNotContains:
            return "Does Not Contain";
    }
    return "Contains";
}

FilterOperation FilterOperationFromLabel(std::string_view label) {
    if (label == "Equals") {
        return FilterOperation::kEquals;
    }
    if (label == "Does Not Contain") {
        return FilterOperation::kNotContains;
    }
    return FilterOperation::kContains;
}

std::string LinkHistoryKindLabel(LinkHistoryKind kind) {
    switch (kind) {
        case LinkHistoryKind::kUrl:
            return "URL";
        case LinkHistoryKind::kAttachment:
            return "Attachment";
        case LinkHistoryKind::kFile:
            return "File";
    }
    return "URL";
}

std::string JoinLines(const std::vector<std::string>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            stream << '\n';
        }
        stream << values[index];
    }
    return stream.str();
}

std::string LowercaseCopy(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (const unsigned char ch : value) {
        lowered.push_back(static_cast<char>(std::tolower(ch)));
    }
    return lowered;
}

std::string TrimWhitespace(std::string_view value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return std::string(value.substr(start, end - start));
}

std::string SlugifyMailboxId(std::string_view label) {
    std::string slug;
    slug.reserve(label.size());
    bool last_dash = false;
    for (const unsigned char ch : label) {
        if (std::isalnum(ch) != 0) {
            slug.push_back(static_cast<char>(std::tolower(ch)));
            last_dash = false;
        } else if (!last_dash) {
            slug.push_back('-');
            last_dash = true;
        }
    }
    while (!slug.empty() && slug.front() == '-') {
        slug.erase(slug.begin());
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    return slug.empty() ? "mailbox" : slug;
}

std::optional<int32> FindCaseInsensitive(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) {
        return std::nullopt;
    }
    const std::string lowered_haystack = LowercaseCopy(haystack);
    const std::string lowered_needle = LowercaseCopy(needle);
    const std::size_t offset = lowered_haystack.find(lowered_needle);
    if (offset == std::string::npos) {
        return std::nullopt;
    }
    return static_cast<int32>(offset);
}

bool EqualsCaseInsensitive(std::string_view left, std::string_view right) {
    return LowercaseCopy(left) == LowercaseCopy(right);
}

int MaxRecentMailboxCount(const IniSettingsStore& settings) {
    return std::clamp(settings.GetInt("Settings", "MaxRecentMailbox", 10), 0, 99);
}

std::string MailboxRowLabel(const MailboxSummary& mailbox) {
    std::string label = mailbox.display_name;
    if (mailbox.unread_count > 0) {
        label += " (" + std::to_string(mailbox.unread_count) + ")";
    }
    return label;
}

std::string SecurityLabel(TransportSecurityMode mode) {
    switch (mode) {
        case TransportSecurityMode::kPlaintext:
            return "Plaintext";
        case TransportSecurityMode::kImplicitTls:
            return "Implicit TLS";
        case TransportSecurityMode::kStartTls:
            return "STARTTLS";
    }
    return "Plaintext";
}

TransportSecurityMode SecurityModeFromLabel(std::string_view label) {
    if (label == "Implicit TLS") {
        return TransportSecurityMode::kImplicitTls;
    }
    if (label == "STARTTLS") {
        return TransportSecurityMode::kStartTls;
    }
    return TransportSecurityMode::kPlaintext;
}

std::string IncomingAuthLabel(const AccountProfile& account) {
    if (account.uses_imap) {
        switch (account.imap_auth) {
            case ImapAuthMode::kPassword:
                return "Password";
            case ImapAuthMode::kKerberos:
                return "Kerberos";
            case ImapAuthMode::kCramMd5:
                return "CRAM-MD5";
            case ImapAuthMode::kOAuth2:
                return "OAuth 2.0";
        }
    }
    switch (account.pop_auth) {
        case PopAuthMode::kPassword:
            return "Password";
        case PopAuthMode::kKerberos:
            return "Kerberos";
        case PopAuthMode::kAPOP:
            return "APOP";
        case PopAuthMode::kRPA:
            return "RPA";
        case PopAuthMode::kOAuth2:
            return "OAuth 2.0";
    }
    return "Password";
}

std::string OutgoingAuthLabel(SmtpAuthMode mode) {
    switch (mode) {
        case SmtpAuthMode::kNone:
            return "None";
        case SmtpAuthMode::kCramMd5:
            return "CRAM-MD5";
        case SmtpAuthMode::kLogin:
            return "LOGIN";
        case SmtpAuthMode::kPlain:
            return "PLAIN";
        case SmtpAuthMode::kOAuth2:
            return "OAuth 2.0";
    }
    return "None";
}

std::string OAuthProviderLabel(OAuthProviderKind kind) {
    switch (kind) {
        case OAuthProviderKind::kNone:
            return "None";
        case OAuthProviderKind::kMicrosoft365:
            return "Microsoft 365";
        case OAuthProviderKind::kGoogle:
            return "Google";
        case OAuthProviderKind::kCustom:
            return "Custom";
    }
    return "None";
}

std::string OAuthMechanismLabel(OAuthAuthMechanism mechanism) {
    switch (mechanism) {
        case OAuthAuthMechanism::kXOAUTH2:
            return "XOAUTH2";
        case OAuthAuthMechanism::kOAUTHBEARER:
            return "OAUTHBEARER";
    }
    return "XOAUTH2";
}

std::string MenuValue(BMenuField* field) {
    if (field == nullptr || field->Menu() == nullptr || field->Menu()->FindMarked() == nullptr) {
        return {};
    }
    return field->Menu()->FindMarked()->Label();
}

BMenuField* BuildMenuField(const char* name,
                           const char* label,
                           const std::vector<std::string>& items) {
    auto* menu = new BPopUpMenu(name);
    for (const auto& item : items) {
        menu->AddItem(new BMenuItem(item.c_str(), nullptr));
    }
    return new BMenuField(name, label, menu);
}

void SetMarkedMenuValue(BMenuField* field, std::string_view value) {
    if (field == nullptr || field->Menu() == nullptr) {
        return;
    }
    for (int32 index = 0; index < field->Menu()->CountItems(); ++index) {
        if (BMenuItem* item = field->Menu()->ItemAt(index); item != nullptr) {
            item->SetMarked(item->Label() == value);
        }
    }
}

std::optional<BRect> ParseFrame(std::string_view serialized) {
    std::istringstream stream(std::string(serialized));
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    char c1 = '\0';
    char c2 = '\0';
    char c3 = '\0';
    if ((stream >> left >> c1 >> top >> c2 >> right >> c3 >> bottom) && c1 == ',' && c2 == ',' &&
        c3 == ',') {
        return BRect(left, top, right, bottom);
    }
    return std::nullopt;
}

std::string SerializeFrame(BRect frame) {
    std::ostringstream stream;
    stream << frame.left << ',' << frame.top << ',' << frame.right << ',' << frame.bottom;
    return stream.str();
}

std::string MailboxProtocolLabel(const MailboxSummary& mailbox) {
    std::ostringstream label;
    label << "Account: " << mailbox.account_id << '\n'
          << "Protocol: " << mailbox.protocol << '\n'
          << "Unread: " << mailbox.unread_count << '\n'
          << "Remote: " << (mailbox.is_remote ? "Yes" : "No");
    return label.str();
}

std::string AttachmentSummary(const SignatureTemplate& entry) {
    return entry.body.plain_text;
}

std::string FormatTimestamp(std::int64_t value) {
    if (value <= 0) {
        return {};
    }

    const std::time_t timestamp = static_cast<std::time_t>(value);
    const std::tm* local_time = std::localtime(&timestamp);
    if (local_time == nullptr) {
        return {};
    }

    char buffer[64];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", local_time) == 0) {
        return {};
    }
    return buffer;
}

std::string FormatFileSize(std::uintmax_t size) {
    if (size >= 1024 * 1024) {
        return std::to_string(static_cast<unsigned long long>(size / (1024 * 1024))) + " MB";
    }
    if (size >= 1024) {
        return std::to_string(static_cast<unsigned long long>(size / 1024)) + " KB";
    }
    return std::to_string(static_cast<unsigned long long>(size)) + " B";
}

bool CopyTextToClipboard(std::string_view text) {
    if (be_clipboard == nullptr) {
        return false;
    }
    if (be_clipboard->Lock() != true) {
        return false;
    }
    BMessage* data = be_clipboard->Data();
    if (data == nullptr) {
        be_clipboard->Unlock();
        return false;
    }
    data->MakeEmpty();
    data->AddData("text/plain",
                  B_MIME_TYPE,
                  text.data(),
                  static_cast<ssize_t>(text.size()));
    const bool committed = be_clipboard->Commit() == B_OK;
    be_clipboard->Unlock();
    return committed;
}

std::string EscapeForOpenCommand(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        if (ch == '"' || ch == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

bool LaunchPath(const std::filesystem::path& path) {
    if (be_roster == nullptr) {
        return false;
    }

    entry_ref ref;
    BEntry entry(path.string().c_str(), true);
    if (entry.GetRef(&ref) != B_OK) {
        return false;
    }
    return be_roster->Launch(&ref) == B_OK;
}

bool LaunchExternalTarget(std::string_view target) {
    if (target.empty()) {
        return false;
    }
    const std::string command = "open \"" + EscapeForOpenCommand(target) + "\" >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}

std::string EscapeShellSingleQuoted(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char ch : value) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(ch);
        }
    }
    return escaped;
}

bool SendPathToPrinter(const std::filesystem::path& path) {
    const std::string escaped = EscapeShellSingleQuoted(path.string());
    const std::string command =
        "(command -v lpr >/dev/null 2>&1 && lpr '" + escaped + "') || "
        "(command -v lp >/dev/null 2>&1 && lp '" + escaped + "') >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}

bool HasPrinterCommand() {
    return std::system("(command -v lpr >/dev/null 2>&1) || (command -v lp >/dev/null 2>&1)") == 0;
}

std::string EscapeHtmlText(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }
    return escaped;
}

bool WriteWholeFile(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return output.good();
}

enum class DirtyPromptAction {
    kCancel,
    kDiscard,
    kSave,
};

DirtyPromptAction PromptToSaveChanges(std::string_view title, std::string_view body) {
    BAlert alert(std::string(title).c_str(),
                 std::string(body).c_str(),
                 "Cancel",
                 "Discard",
                 "Save",
                 B_WIDTH_AS_USUAL,
                 B_WARNING_ALERT);
    alert.SetShortcut(0, B_ESCAPE);
    const int32 choice = alert.Go();
    if (choice == 2) {
        return DirtyPromptAction::kSave;
    }
    if (choice == 1) {
        return DirtyPromptAction::kDiscard;
    }
    return DirtyPromptAction::kCancel;
}

std::string SerializeSplitWeights(BSplitView& split_view) {
    std::ostringstream stream;
    for (int32 index = 0; index < split_view.CountChildren(); ++index) {
        if (index != 0) {
            stream << ',';
        }
        stream << split_view.ItemWeight(index);
    }
    return stream.str();
}

void RestoreSplitWeights(BSplitView& split_view, std::string_view serialized) {
    if (serialized.empty()) {
        return;
    }
    std::istringstream stream(std::string(serialized));
    float weight = 0.0f;
    char comma = '\0';
    int32 index = 0;
    while (stream >> weight) {
        if (index < split_view.CountChildren()) {
            split_view.SetItemWeight(index, weight, false);
        }
        ++index;
        if (!(stream >> comma) || comma != ',') {
            break;
        }
    }
    split_view.InvalidateLayout();
}

std::string BytesToHex(const void* data, std::size_t size) {
    static constexpr char kHex[] = "0123456789abcdef";
    const auto* bytes = static_cast<const uint8_t*>(data);
    std::string value;
    value.resize(size * 2);
    for (std::size_t index = 0; index < size; ++index) {
        value[index * 2] = kHex[(bytes[index] >> 4) & 0x0f];
        value[index * 2 + 1] = kHex[bytes[index] & 0x0f];
    }
    return value;
}

std::optional<std::vector<uint8_t>> HexToBytes(std::string_view hex) {
    if ((hex.size() % 2) != 0) {
        return std::nullopt;
    }

    auto decode_nibble = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return 10 + (ch - 'a');
        }
        if (ch >= 'A' && ch <= 'F') {
            return 10 + (ch - 'A');
        }
        return -1;
    };

    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t index = 0; index < hex.size(); index += 2) {
        const int high = decode_nibble(hex[index]);
        const int low = decode_nibble(hex[index + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        bytes.push_back(static_cast<uint8_t>((high << 4) | low));
    }
    return bytes;
}

std::string SerializeColumnListState(BColumnListView& list_view) {
    BMessage archive;
    list_view.SaveState(&archive);
    const ssize_t flattened_size = archive.FlattenedSize();
    if (flattened_size <= 0) {
        return {};
    }

    std::vector<uint8_t> buffer(static_cast<std::size_t>(flattened_size));
    if (archive.Flatten(reinterpret_cast<char*>(buffer.data()), flattened_size) != B_OK) {
        return {};
    }
    return BytesToHex(buffer.data(), buffer.size());
}

void RestoreColumnListState(BColumnListView& list_view, std::string_view serialized) {
    if (serialized.empty()) {
        return;
    }

    const auto bytes = HexToBytes(serialized);
    if (!bytes || bytes->empty()) {
        return;
    }

    BMessage archive;
    if (archive.Unflatten(B_MESSAGE_TYPE, bytes->data(), static_cast<ssize_t>(bytes->size())) != B_OK) {
        return;
    }
    list_view.LoadState(&archive);
}

class ContextListView final : public BListView {
public:
    using ContextHandler = std::function<void(BPoint)>;

    ContextListView(const char* name,
                    BMessage* selection_message,
                    BMessage* invocation_message,
                    uint32 delete_message_what,
                    ContextHandler handler)
        : BListView(name),
          delete_message_what_(delete_message_what),
          handler_(std::move(handler)) {
        if (selection_message != nullptr) {
            SetSelectionMessage(selection_message);
        }
        if (invocation_message != nullptr) {
            SetInvocationMessage(invocation_message);
        }
    }

    void MouseDown(BPoint where) override {
        uint32 buttons = 0;
        GetMouse(&where, &buttons, false);
        if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
            const int32 index = IndexOf(where);
            if (index >= 0) {
                Select(index);
            }
            if (handler_) {
                handler_(where);
            }
            return;
        }
        BListView::MouseDown(where);
    }

    void KeyDown(const char* bytes, int32 num_bytes) override {
        if (bytes != nullptr && num_bytes == 1 && bytes[0] == B_DELETE && delete_message_what_ != 0 &&
            Target() != nullptr && Looper() != nullptr) {
            BMessage command(delete_message_what_);
            Looper()->PostMessage(&command, Target());
            return;
        }
        BListView::KeyDown(bytes, num_bytes);
    }

private:
    uint32 delete_message_what_ = 0;
    ContextHandler handler_;
};

class ContextColumnListView final : public BColumnListView {
public:
    using ContextHandler = std::function<void(BPoint)>;

    ContextColumnListView(const char* name,
                          BMessage* selection_message,
                          BMessage* invocation_message,
                          uint32 delete_message_what,
                          ContextHandler handler)
        : BColumnListView(name, B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE),
          delete_message_what_(delete_message_what),
          handler_(std::move(handler)) {
        SetSelectionMode(B_SINGLE_SELECTION_LIST);
        SetSelectionMessage(selection_message);
        SetInvocationMessage(invocation_message);
        SetSortingEnabled(true);
        SetColumnFlags(static_cast<column_flags>(B_ALLOW_COLUMN_MOVE | B_ALLOW_COLUMN_RESIZE | B_ALLOW_COLUMN_POPUP));
    }

    void MouseDown(BPoint where) override {
        uint32 buttons = 0;
        GetMouse(&where, &buttons, false);
        if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
            if (BRow* row = RowAt(where)) {
                SetFocusRow(row, true);
            } else {
                DeselectAll();
            }
            if (handler_) {
                handler_(where);
            }
            return;
        }
        BColumnListView::MouseDown(where);
    }

    void KeyDown(const char* bytes, int32 num_bytes) override {
        if (bytes != nullptr && num_bytes == 1 && bytes[0] == B_DELETE && delete_message_what_ != 0 &&
            Target() != nullptr && Looper() != nullptr) {
            BMessage command(delete_message_what_);
            Looper()->PostMessage(&command, Target());
            return;
        }
        BColumnListView::KeyDown(bytes, num_bytes);
    }

private:
    uint32 delete_message_what_ = 0;
    ContextHandler handler_;
};

class ContextOutlineListView final : public BOutlineListView {
public:
    using ContextHandler = std::function<void(BPoint)>;

    ContextOutlineListView(const char* name,
                           BMessage* selection_message,
                           BMessage* invocation_message,
                           uint32 delete_message_what,
                           ContextHandler handler)
        : BOutlineListView(name),
          delete_message_what_(delete_message_what),
          handler_(std::move(handler)) {
        if (selection_message != nullptr) {
            SetSelectionMessage(selection_message);
        }
        if (invocation_message != nullptr) {
            SetInvocationMessage(invocation_message);
        }
    }

    void MouseDown(BPoint where) override {
        uint32 buttons = 0;
        GetMouse(&where, &buttons, false);
        if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
            const int32 index = IndexOf(where);
            if (index >= 0) {
                Select(index);
            } else {
                DeselectAll();
            }
            if (handler_) {
                handler_(where);
            }
            return;
        }
        BOutlineListView::MouseDown(where);
    }

    void KeyDown(const char* bytes, int32 num_bytes) override {
        if (bytes != nullptr && num_bytes == 1 && bytes[0] == B_DELETE && delete_message_what_ != 0 &&
            Target() != nullptr && Looper() != nullptr) {
            BMessage command(delete_message_what_);
            Looper()->PostMessage(&command, Target());
            return;
        }
        BOutlineListView::KeyDown(bytes, num_bytes);
    }

private:
    uint32 delete_message_what_ = 0;
    ContextHandler handler_;
};

class TextPromptWindow final : public BWindow {
public:
    TextPromptWindow(const char* title,
                     const char* label,
                     std::string initial_value,
                     const char* response_key,
                     BMessenger target,
                     BMessage payload)
        : BWindow(BRect(220, 220, 520, 320),
                  title,
                  B_TITLED_WINDOW,
                  B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
          response_key_(response_key == nullptr ? "" : response_key),
          target_(std::move(target)),
          payload_(std::move(payload)) {
        field_ = new BTextControl("wazoo-prompt-text", label, initial_value.c_str(), nullptr);
        auto* ok = new BButton("wazoo-prompt-ok", "OK", new BMessage(kPaneSaveMessage));
        auto* cancel = new BButton("wazoo-prompt-cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
        ok->MakeDefault(true);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(field_)
            .AddGroup(B_HORIZONTAL, 8)
                .AddGlue()
                .Add(cancel)
                .Add(ok)
            .End();
    }

    void MessageReceived(BMessage* message) override {
        if (message != nullptr && message->what == kPaneSaveMessage) {
            if (!response_key_.empty()) {
                payload_.AddString(response_key_.c_str(),
                                   field_ != nullptr && field_->Text() != nullptr ? field_->Text() : "");
            }
            target_.SendMessage(&payload_);
            Quit();
            return;
        }
        BWindow::MessageReceived(message);
    }

private:
    std::string response_key_;
    BMessenger target_;
    BMessage payload_;
    BTextControl* field_ = nullptr;
};

class WazooTabView final : public BTabView {
public:
    explicit WazooTabView(HaikuWazooWindow* owner, const char* name)
        : BTabView(name),
          owner_(owner) {}

    void Select(int32 tab) override {
        if (owner_ == nullptr || owner_->RequestTabSelection(tab)) {
            BTabView::Select(tab);
            if (owner_ != nullptr) {
                owner_->HandleTabSelectionCommitted();
            }
        }
    }

private:
    HaikuWazooWindow* owner_ = nullptr;
};

std::string FormatTaskProgress(const MailTaskRecord& task) {
    if (task.total > 0) {
        const int total = std::max(1, task.total);
        const int so_far = std::clamp(task.so_far, 0, total);
        const int percent = static_cast<int>((100.0 * so_far) / total);
        const int filled = std::clamp((so_far * 10) / total, 0, 10);
        std::string bar = "[";
        for (int index = 0; index < 10; ++index) {
            bar += index < filled ? "#" : ".";
        }
        bar += "] ";
        bar += std::to_string(percent);
        bar += "%";
        return bar;
    }
    return task.status.empty() ? "Pending" : task.status;
}

class WazooPaneBase : public BGroupView {
public:
    WazooPaneBase(HaikuShellHost& shell_host, std::string tool_id)
        : BGroupView(B_VERTICAL),
          shell_host_(shell_host),
          tool_id_(std::move(tool_id)) {}

    virtual ~WazooPaneBase() = default;

    virtual void Refresh() = 0;
    virtual void PersistState() {}
    virtual bool HandleCommand(uint32 command_id) {
        return false;
    }
    virtual bool IsCommandEnabled(uint32 command_id) const {
        return false;
    }
    virtual bool CanFind() const {
        return false;
    }
    virtual bool CanFindAgain() const {
        return CanFind();
    }
    virtual bool HandleFind(std::string_view term, bool repeat) {
        (void)term;
        (void)repeat;
        return false;
    }
    virtual bool CanPrintPreview() const {
        return false;
    }
    virtual bool CanDirectPrint() const {
        return false;
    }
    virtual bool HandlePrint(bool preview) {
        (void)preview;
        return false;
    }
    virtual BView* PreferredFocusView() const {
        return nullptr;
    }
    virtual bool CanDeactivate() {
        return true;
    }
    const std::string& ToolId() const { return tool_id_; }

protected:
    HaikuShellHost& shell_host_;
    std::string tool_id_;
};

uint32_t RouteWazooCommand(std::string_view tool_id, uint32_t command_id) {
    if (tool_id == "mailboxes") {
        switch (command_id) {
            case kPaneActionMessage:
                return kMailboxOpenMessage;
            case kPaneSaveMessage:
                return kMailboxRenameMessage;
            case kPanePrimaryCommandMessage:
                return kMailboxFindMessagesMessage;
            case kPaneSecondaryCommandMessage:
                return kMailboxRefreshMessage;
            default:
                return command_id;
        }
    }
    if (tool_id == "signatures") {
        switch (command_id) {
            case kPaneNewMessage:
                return kSignatureNewMessage;
            case kPaneDuplicateGenericMessage:
                return kSignatureDuplicateMessage;
            case kPaneSaveMessage:
                return kSignatureSaveMessage;
            case kPaneDeleteMessage:
                return kSignatureDeleteMessage;
            case kPaneSecondaryCommandMessage:
                return kSignatureRevealMessage;
            default:
                return command_id;
        }
    }
    if (tool_id == "stationery") {
        switch (command_id) {
            case kPaneNewMessage:
                return kStationeryNewMessage;
            case kPaneDuplicateGenericMessage:
                return kStationeryDuplicateMessage;
            case kPaneSaveMessage:
                return kStationerySaveMessage;
            case kPaneDeleteMessage:
                return kStationeryDeleteMessage;
            case kPanePrimaryCommandMessage:
                return kStationeryUseMessage;
            case kPaneSecondaryCommandMessage:
                return kStationeryReplyMessage;
            default:
                return command_id;
        }
    }
    if (tool_id == "link-history") {
        switch (command_id) {
            case kPanePrimaryCommandMessage:
            case kPaneActionMessage:
                return kLinkHistoryOpenMessage;
            case kPaneDeleteMessage:
                return kLinkHistoryRemoveMessage;
            case kPaneSecondaryCommandMessage:
                return kLinkHistoryClearMessage;
            default:
                return command_id;
        }
    }
    if (tool_id == "nicknames") {
        switch (command_id) {
            case kPaneDuplicateGenericMessage:
                return kNicknameDuplicateMessage;
            case kPanePrimaryCommandMessage:
                return kNicknameComposeMessage;
            case kPaneSecondaryCommandMessage:
                return kWazooToggleDetailsMessage;
            default:
                return command_id;
        }
    }
    return command_id;
}

#if 0
// Legacy placeholder panes kept only as porting reference. The live shell uses
// the dedicated command-routed pane implementations below.
class MailboxesPane final : public WazooPaneBase {
public:
    explicit MailboxesPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "mailboxes") {
        summary_view_ = new BStringView("mailboxes-summary", "Mailboxes");
        list_view_ = new BOutlineListView("mailboxes-outline");
        list_view_->SetSelectionMessage(new BMessage(kPaneSelectionMessage));
        list_view_->SetInvocationMessage(new BMessage(kPaneInvokeMessage));
        detail_view_ = new BTextView("mailboxes-detail");
        detail_view_->MakeEditable(false);
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);
        auto* open_button = new BButton("mailboxes-open", "Open Mailbox", new BMessage(kMailboxOpenMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(summary_view_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new BScrollView("mailboxes-scroll", list_view_, 0, false, true), 0.42f)
                .Add(new BScrollView("mailboxes-detail-scroll", detail_view_, 0, false, true), 0.58f)
            .End()
            .AddGroup(B_HORIZONTAL, 8)
                .Add(open_button)
                .AddGlue()
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kPaneSelectionMessage:
                UpdateDetail();
                return;
            case kPaneInvokeMessage:
            case kPaneActionMessage:
                ActivateSelectedMailbox();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        list_view_->MakeEmpty();
        mailbox_ids_.clear();
        labels_.clear();
        labels_.clear();

        auto mailboxes = shell_host_.Workspace().Mailboxes();
        std::stable_sort(mailboxes.begin(), mailboxes.end(), [](const MailboxSummary& left, const MailboxSummary& right) {
            if (left.account_id != right.account_id) {
                return left.account_id < right.account_id;
            }
            if (left.system_mailbox != right.system_mailbox) {
                return left.system_mailbox && !right.system_mailbox;
            }
            return left.display_name < right.display_name;
        });

        std::map<std::string, std::string> parent_by_id;
        for (const auto& mailbox : mailboxes) {
            parent_by_id.emplace(mailbox.id, mailbox.parent_id);
        }

        const auto depth_for = [&parent_by_id](const std::string& mailbox_id) {
            int32 depth = 0;
            auto current = parent_by_id.find(mailbox_id);
            while (current != parent_by_id.end() && !current->second.empty()) {
                ++depth;
                if (current->second.rfind("account:", 0) == 0) {
                    break;
                }
                current = parent_by_id.find(current->second);
            }
            return depth;
        };

        int32 selected_index = -1;
        for (const auto& mailbox : mailboxes) {
            std::string label = mailbox.display_name;
            if (mailbox.unread_count > 0) {
                label += " (" + std::to_string(mailbox.unread_count) + ")";
            }
            list_view_->AddItem(new BStringItem(label.c_str(), depth_for(mailbox.id), false));
            mailbox_ids_.push_back(mailbox.id);
            labels_.push_back(label);
            if (mailbox.id == shell_host_.ActiveMailboxId()) {
                selected_index = static_cast<int32>(mailbox_ids_.size() - 1);
            }
        }
        if (selected_index < 0 && !mailbox_ids_.empty()) {
            selected_index = 0;
        }
        if (selected_index >= 0) {
            list_view_->Select(selected_index);
        }
        UpdateDetail();
    }

    bool CanFind() const override {
        return !mailbox_ids_.empty();
    }

    bool HandleFind(std::string_view term, bool repeat) override {
        if (term.empty() || mailbox_ids_.empty()) {
            return false;
        }

        const int32 current = list_view_->CurrentSelection();
        std::size_t start = 0;
        if (repeat && current >= 0) {
            start = static_cast<std::size_t>(current + 1);
        } else if (!repeat && current >= 0) {
            start = static_cast<std::size_t>(current);
        }

        for (std::size_t offset = 0; offset < labels_.size(); ++offset) {
            const std::size_t index = (start + offset) % labels_.size();
            if (FindCaseInsensitive(labels_[index], term)) {
                list_view_->Select(static_cast<int32>(index));
                list_view_->ScrollToSelection();
                UpdateDetail();
                list_view_->MakeFocus(true);
                return true;
            }
        }
        return false;
    }

    BView* PreferredFocusView() const override {
        return list_view_;
    }

private:
    void UpdateDetail() {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= mailbox_ids_.size()) {
            detail_view_->SetText("Select a mailbox.");
            return;
        }
        const auto mailboxes = shell_host_.Workspace().Mailboxes();
        const auto it = std::find_if(mailboxes.begin(), mailboxes.end(), [&](const auto& mailbox) {
            return mailbox.id == mailbox_ids_[static_cast<std::size_t>(index)];
        });
        detail_view_->SetText(it == mailboxes.end() ? "" : MailboxProtocolLabel(*it).c_str());
    }

    void ActivateSelectedMailbox() {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= mailbox_ids_.size()) {
            return;
        }
        shell_host_.OpenMailbox(mailbox_ids_[static_cast<std::size_t>(index)]);
        shell_host_.ShowMainWindow();
    }

    BStringView* summary_view_ = nullptr;
    BOutlineListView* list_view_ = nullptr;
    BTextView* detail_view_ = nullptr;
    std::vector<std::string> mailbox_ids_;
    std::vector<std::string> labels_;
};
#endif

class TaskStatusPane final : public WazooPaneBase {
public:
    explicit TaskStatusPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "task-status") {
        summary_view_ = new BStringView("task-status-summary", "Task Status");
        list_view_ = new ContextColumnListView(
            "task-status-list",
            new BMessage(kPaneSelectionMessage),
            new BMessage(kPaneInvokeMessage),
            0,
            [this](BPoint where) { ShowContextMenu(where); });
        list_view_->AddColumn(
            new BStringColumn("Task", 200.0f, 120.0f, 420.0f, B_TRUNCATE_END), kTaskFieldTask);
        list_view_->AddColumn(
            new BStringColumn("Persona", 128.0f, 72.0f, 280.0f, B_TRUNCATE_END), kTaskFieldPersona);
        list_view_->AddColumn(
            new BStringColumn("Status", 128.0f, 72.0f, 240.0f, B_TRUNCATE_END), kTaskFieldStatus);
        list_view_->AddColumn(
            new BStringColumn("Details", 260.0f, 120.0f, 520.0f, B_TRUNCATE_END), kTaskFieldDetails);
        list_view_->AddColumn(
            new BStringColumn("Progress", 110.0f, 84.0f, 180.0f, B_TRUNCATE_END), kTaskFieldProgress);
        RestoreColumnListState(*list_view_, GuiPreferencesFromSettings(shell_host_.Settings()).task_column_layout);
        detail_view_ = new BTextView("task-status-detail");
        detail_view_->MakeEditable(false);
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);

        auto* retry_button = new BButton("task-status-retry", "Retry", new BMessage(kPaneSaveMessage));
        auto* cancel_button = new BButton("task-status-cancel", "Cancel", new BMessage(kPaneDeleteMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(summary_view_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new BScrollView("task-status-scroll", list_view_, 0, false, true), 0.54f)
                .Add(new BScrollView("task-status-detail-scroll", detail_view_, 0, false, true), 0.46f)
            .End()
            .AddGroup(B_HORIZONTAL, 8)
                .Add(retry_button)
                .Add(cancel_button)
                .AddGlue()
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
    }

    void PersistState() override {
        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        preferences.task_column_layout = SerializeColumnListState(*list_view_);
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kPaneSelectionMessage:
                UpdateDetail();
                return;
            case kPaneInvokeMessage:
                UpdateDetail();
                return;
            case kPaneSaveMessage:
                RetrySelected();
                return;
            case kPaneDeleteMessage:
                CancelSelected();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        list_view_->Clear();
        entry_ids_.clear();
        entry_is_imap_action_.clear();

        for (const auto& task : shell_host_.Tasks().Tasks()) {
            auto* row = new BRow();
            row->SetField(new BStringField(task.title.c_str()), kTaskFieldTask);
            row->SetField(new BStringField(task.persona.c_str()), kTaskFieldPersona);
            row->SetField(new BStringField(task.status.c_str()), kTaskFieldStatus);
            row->SetField(new BStringField(task.details.c_str()), kTaskFieldDetails);
            row->SetField(new BStringField(FormatTaskProgress(task).c_str()), kTaskFieldProgress);
            list_view_->AddRow(row);
            entry_ids_.push_back(task.id);
            entry_is_imap_action_.push_back(false);
        }
        for (const auto& action : shell_host_.QueuedImapActions()) {
            std::string kind_label;
            switch (action.kind) {
                case ImapActionKind::kDelete:
                    kind_label = "Delete";
                    break;
                case ImapActionKind::kUndelete:
                    kind_label = "Undelete";
                    break;
                case ImapActionKind::kExpungeMailbox:
                    kind_label = "Purge/Expunge";
                    break;
                case ImapActionKind::kMove:
                    kind_label = "Move";
                    break;
                case ImapActionKind::kCopy:
                    kind_label = "Copy";
                    break;
                case ImapActionKind::kCreateMailbox:
                    kind_label = "Create Mailbox";
                    break;
                case ImapActionKind::kRenameMailbox:
                    kind_label = "Rename Mailbox";
                    break;
                case ImapActionKind::kDeleteMailbox:
                    kind_label = "Delete Mailbox";
                    break;
                case ImapActionKind::kFetchAttachment:
                    kind_label = "Fetch Attachment";
                    break;
                case ImapActionKind::kFetchDefaultMessage:
                    kind_label = "Fetch Default Message";
                    break;
                case ImapActionKind::kFetchFullMessage:
                    kind_label = "Fetch Full Message";
                    break;
                case ImapActionKind::kRedownloadDefaultMessage:
                    kind_label = "Redownload Default Message";
                    break;
                case ImapActionKind::kRedownloadFullMessage:
                    kind_label = "Redownload Full Message";
                    break;
                case ImapActionKind::kResyncMailbox:
                    kind_label = "Resync Mailbox";
                    break;
                case ImapActionKind::kRefreshMailboxList:
                    kind_label = "Refresh Mailboxes";
                    break;
            }
            auto* row = new BRow();
            row->SetField(new BStringField(kind_label.c_str()), kTaskFieldTask);
            row->SetField(new BStringField(action.account_id.c_str()), kTaskFieldPersona);
            row->SetField(
                new BStringField(action.last_error.empty() ? "Pending" : "Failed"), kTaskFieldStatus);
            row->SetField(new BStringField(
                              action.last_error.empty() ? action.remote_mailbox.c_str()
                                                        : action.last_error.c_str()),
                          kTaskFieldDetails);
            row->SetField(new BStringField(action.id.c_str()), kTaskFieldProgress);
            list_view_->AddRow(row);
            entry_ids_.push_back(action.id);
            entry_is_imap_action_.push_back(true);
        }

        if (!entry_ids_.empty()) {
            if (BRow* row = list_view_->RowAt(0)) {
                list_view_->SetFocusRow(row, true);
            }
        }
        UpdateDetail();
    }

private:
    void UpdateDetail() {
        const BRow* selected = list_view_->CurrentSelection();
        const int32 index = selected == nullptr ? -1 : list_view_->IndexOf(selected);
        if (index < 0 || static_cast<std::size_t>(index) >= entry_ids_.size()) {
            detail_view_->SetText("No task selected.");
            return;
        }
        if (!entry_is_imap_action_[static_cast<std::size_t>(index)]) {
            const auto tasks = shell_host_.Tasks().Tasks();
            const auto it = std::find_if(tasks.begin(), tasks.end(), [&](const auto& task) {
                return task.id == entry_ids_[static_cast<std::size_t>(index)];
            });
            if (it != tasks.end()) {
                std::ostringstream detail;
                detail << it->title << "\nStatus: " << it->status << "\nProgress: "
                       << FormatTaskProgress(*it) << "\nDetails: " << it->details;
                detail_view_->SetText(detail.str().c_str());
                return;
            }
        } else {
            const auto actions = shell_host_.QueuedImapActions();
            const auto it = std::find_if(actions.begin(), actions.end(), [&](const auto& action) {
                return action.id == entry_ids_[static_cast<std::size_t>(index)];
            });
            if (it != actions.end()) {
                std::ostringstream detail;
                detail << "Action: " << it->id << "\nMailbox: " << it->mailbox_id
                       << "\nRemote: " << it->remote_mailbox << "\nError: " << it->last_error;
                detail_view_->SetText(detail.str().c_str());
                return;
            }
        }
        detail_view_->SetText("Task details unavailable.");
    }

    void RetrySelected() {
        const BRow* selected = list_view_->CurrentSelection();
        const int32 index = selected == nullptr ? -1 : list_view_->IndexOf(selected);
        if (index >= 0 && static_cast<std::size_t>(index) < entry_ids_.size() &&
            entry_is_imap_action_[static_cast<std::size_t>(index)]) {
            shell_host_.RetryTask(entry_ids_[static_cast<std::size_t>(index)]);
            Refresh();
        }
    }

    void CancelSelected() {
        const BRow* selected = list_view_->CurrentSelection();
        const int32 index = selected == nullptr ? -1 : list_view_->IndexOf(selected);
        if (index >= 0 && static_cast<std::size_t>(index) < entry_ids_.size() &&
            entry_is_imap_action_[static_cast<std::size_t>(index)]) {
            shell_host_.CancelTask(entry_ids_[static_cast<std::size_t>(index)]);
            Refresh();
        }
    }

    void ShowContextMenu(BPoint where) {
        BPopUpMenu menu("task-status-context", false, false);
        menu.AddItem(new BMenuItem("Retry", new BMessage(kPaneSaveMessage)));
        menu.AddItem(new BMenuItem("Cancel", new BMessage(kPaneDeleteMessage)));
        menu.SetTargetForItems(this);
        list_view_->ConvertToScreen(&where);
        menu.Go(where, true, false, true);
    }

    BStringView* summary_view_ = nullptr;
    ContextColumnListView* list_view_ = nullptr;
    BTextView* detail_view_ = nullptr;
    std::vector<std::string> entry_ids_;
    std::vector<bool> entry_is_imap_action_;
};

class TaskErrorsPane final : public WazooPaneBase {
public:
    explicit TaskErrorsPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "task-errors") {
        list_view_ = new ContextListView("task-errors-list",
                                         new BMessage(kPaneSelectionMessage),
                                         new BMessage(kPaneInvokeMessage),
                                         kPaneDeleteMessage,
                                         [this](BPoint where) { ShowContextMenu(where); });
        detail_view_ = new BTextView("task-errors-detail");
        detail_view_->MakeEditable(false);
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(new BStringView("task-errors-summary", "Task Errors"))
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new BScrollView("task-errors-scroll", list_view_, 0, false, true), 0.40f)
                .Add(new BScrollView("task-errors-detail-scroll", detail_view_, 0, false, true), 0.60f)
            .End();
        auto* remove_button = new BButton("task-errors-remove", "Remove", new BMessage(kPaneDeleteMessage));
        auto* clear_button = new BButton("task-errors-clear", "Remove All", new BMessage(kPaneActionMessage));
        auto* copy_button = new BButton("task-errors-copy", "Copy Details", new BMessage(kPaneSaveMessage));
        AddChild(BLayoutBuilder::Group<>(B_HORIZONTAL, 8)
                     .Add(remove_button)
                     .Add(clear_button)
                     .Add(copy_button)
                     .AddGlue()
                     .View());
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kPaneSelectionMessage:
                UpdateDetail();
                return;
            case kPaneInvokeMessage:
                OpenSelectedError();
                return;
            case kPaneDeleteMessage:
                RemoveSelectedError();
                return;
            case kPaneActionMessage:
                ClearAllErrors();
                return;
            case kPaneSaveMessage:
                CopySelectedDetail();
                return;
            default:
                break;
        }
        BGroupView::MessageReceived(message);
    }

    void Refresh() override {
        list_view_->MakeEmpty();
        details_.clear();
        removable_error_indices_.clear();
        const auto task_errors = shell_host_.Tasks().Errors();
        for (std::size_t index = 0; index < task_errors.size(); ++index) {
            const auto& error = task_errors[index];
            std::string summary = error.task_id + " [" + ToString(error.kind);
            if (!error.mechanism.empty()) {
                summary += "/" + error.mechanism;
            }
            summary += "]";
            list_view_->AddItem(new BStringItem(summary.c_str()));
            details_.push_back(error.message);
            removable_error_indices_.push_back(index);
        }
        for (const auto& action : shell_host_.QueuedImapActions()) {
            if (!action.last_error.empty()) {
                const std::string summary = action.id + " [IMAP action]";
                list_view_->AddItem(new BStringItem(summary.c_str()));
                details_.push_back(action.last_error);
                removable_error_indices_.push_back(std::nullopt);
            }
        }
        if (!details_.empty()) {
            list_view_->Select(0);
        }
        UpdateDetail();
    }

private:
    void UpdateDetail() {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= details_.size()) {
            detail_view_->SetText("No task errors.");
            return;
        }
        detail_view_->SetText(details_[static_cast<std::size_t>(index)].c_str());
    }

    void OpenSelectedError() {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= details_.size()) {
            return;
        }
        ShowInfoAlert("Task Error", details_[static_cast<std::size_t>(index)]);
    }

    void RemoveSelectedError() {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= removable_error_indices_.size()) {
            return;
        }
        const auto removable = removable_error_indices_[static_cast<std::size_t>(index)];
        if (!removable) {
            ShowInfoAlert("Task Error", "This IMAP action error stays with the queued action until that action is retried or cleared.");
            return;
        }
        if (shell_host_.Tasks().RemoveError(*removable)) {
            Refresh();
        }
    }

    void ClearAllErrors() {
        shell_host_.Tasks().ClearErrors();
        Refresh();
    }

    void CopySelectedDetail() {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= details_.size()) {
            return;
        }
        (void)CopyTextToClipboard(details_[static_cast<std::size_t>(index)]);
    }

    void ShowContextMenu(BPoint where) {
        BPopUpMenu menu("task-errors-context", false, false);
        menu.AddItem(new BMenuItem("Open", new BMessage(kPaneInvokeMessage)));
        menu.AddItem(new BMenuItem("Copy Details", new BMessage(kPaneSaveMessage)));
        menu.AddSeparatorItem();
        menu.AddItem(new BMenuItem("Remove", new BMessage(kPaneDeleteMessage)));
        menu.AddItem(new BMenuItem("Remove All", new BMessage(kPaneActionMessage)));
        menu.SetTargetForItems(this);
        list_view_->ConvertToScreen(&where);
        menu.Go(where, true, false, true);
    }

    ContextListView* list_view_ = nullptr;
    BTextView* detail_view_ = nullptr;
    std::vector<std::string> details_;
    std::vector<std::optional<std::size_t>> removable_error_indices_;
};

#if 0
class EditorPaneBase : public WazooPaneBase {
public:
    struct ItemRecord {
        std::string id;
        std::string label;
        std::string detail;
    };

    EditorPaneBase(HaikuShellHost& shell_host, std::string tool_id, std::string title)
        : WazooPaneBase(shell_host, std::move(tool_id)) {
        summary_view_ = new BStringView("tool-summary", title.c_str());
        name_control_ = new BTextControl("tool-name", "Name", "", nullptr);
        aux_control_ = new BTextControl("tool-aux", "Search", "", nullptr);
        item_list_ = new BListView("tool-items");
        item_list_->SetSelectionMessage(new BMessage(kPaneSelectionMessage));
        detail_view_ = new BTextView("tool-detail");
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);
        save_button_ = new BButton("tool-save", "Save", new BMessage(kPaneSaveMessage));
        delete_button_ = new BButton("tool-delete", "Delete", new BMessage(kPaneDeleteMessage));
        action_button_ = new BButton("tool-action", "Refresh", new BMessage(kPaneActionMessage));
        new_button_ = new BButton("tool-new", "New", new BMessage(kPaneNewMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(summary_view_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(name_control_)
                .Add(aux_control_)
            .End()
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new BScrollView("tool-list-scroll", item_list_, 0, false, true), 0.38f)
                .Add(new BScrollView("tool-detail-scroll", detail_view_, 0, false, true), 0.62f)
            .End()
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new_button_)
                .Add(save_button_)
                .Add(delete_button_)
                .AddGlue()
                .Add(action_button_)
            .End();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        item_list_->SetTarget(this);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kPaneSelectionMessage:
                UpdateDetailFromSelection();
                return;
            case kPaneSaveMessage:
                SaveCurrent();
                return;
            case kPaneDeleteMessage:
                DeleteCurrent();
                return;
            case kPaneActionMessage:
                PerformAction();
                return;
            case kPaneNewMessage:
                NewItem();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        items_.clear();
        item_list_->MakeEmpty();
        detail_view_->SetText("");
        PopulateItems();
        for (const auto& item : items_) {
            item_list_->AddItem(new BStringItem(item.label.c_str()));
        }
        if (!items_.empty()) {
            item_list_->Select(0);
            UpdateDetailFromSelection();
        } else {
            name_control_->SetText("");
            detail_view_->SetText("");
        }
    }

protected:
    void ConfigurePane(const char* title,
                       bool show_name,
                       bool show_aux,
                       bool show_save,
                       bool show_delete,
                       bool show_new,
                       const char* action_label) {
        summary_view_->SetText(title);
        if (show_name) {
            name_control_->Show();
        } else {
            name_control_->Hide();
        }
        if (show_aux) {
            aux_control_->Show();
        } else {
            aux_control_->Hide();
        }
        if (show_save) {
            save_button_->Show();
        } else {
            save_button_->Hide();
        }
        if (show_delete) {
            delete_button_->Show();
        } else {
            delete_button_->Hide();
        }
        if (show_new) {
            new_button_->Show();
        } else {
            new_button_->Hide();
        }
        action_button_->SetLabel(action_label);
    }

    void PushItem(std::string id, std::string label, std::string detail) {
        items_.push_back({std::move(id), std::move(label), std::move(detail)});
    }

    std::optional<ItemRecord> SelectedItem() const {
        const int32 index = item_list_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= items_.size()) {
            return std::nullopt;
        }
        return items_[static_cast<std::size_t>(index)];
    }

    std::string CurrentName() const {
        return name_control_->Text() != nullptr ? name_control_->Text() : "";
    }

    std::string CurrentDetail() const {
        return detail_view_->Text() != nullptr ? detail_view_->Text() : "";
    }

    std::string CurrentAux() const {
        return aux_control_->Text() != nullptr ? aux_control_->Text() : "";
    }

    virtual void PopulateItems() = 0;

    virtual void SaveCurrent() {
        Refresh();
    }

    virtual void DeleteCurrent() {
        Refresh();
    }

    virtual void PerformAction() {
        Refresh();
    }

    virtual void NewItem() {
        name_control_->SetText("");
        detail_view_->SetText("");
        item_list_->DeselectAll();
    }

    void UpdateDetailFromSelection() {
        const auto selected = SelectedItem();
        if (!selected) {
            name_control_->SetText("");
            detail_view_->SetText("");
            return;
        }
        name_control_->SetText(selected->id.c_str());
        detail_view_->SetText(selected->detail.c_str());
    }

    BStringView* summary_view_ = nullptr;
    BTextControl* name_control_ = nullptr;
    BTextControl* aux_control_ = nullptr;
    BListView* item_list_ = nullptr;
    BTextView* detail_view_ = nullptr;
    BButton* save_button_ = nullptr;
    BButton* delete_button_ = nullptr;
    BButton* action_button_ = nullptr;
    BButton* new_button_ = nullptr;
    std::vector<ItemRecord> items_;
};

class SignaturesPane final : public EditorPaneBase {
public:
    explicit SignaturesPane(HaikuShellHost& shell_host)
        : EditorPaneBase(shell_host, "signatures", "Signatures") {
        Refresh();
    }

protected:
    void PopulateItems() override {
        ConfigurePane("Signatures", true, false, true, true, true, "Refresh");
        for (const auto& signature : shell_host_.Signatures().Templates()) {
            PushItem(signature.name, signature.name, AttachmentSummary(signature));
        }
    }

    void SaveCurrent() override {
        SignatureTemplate signature;
        signature.name = CurrentName();
        signature.body.plain_text = CurrentDetail();
        std::string error_message;
        shell_host_.Signatures().SaveTemplate(signature, &error_message);
        shell_host_.Signatures().Discover(shell_host_.Signatures().RootDirectory(), &error_message);
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void DeleteCurrent() override {
        const auto selected = SelectedItem();
        if (!selected) {
            return;
        }
        std::string error_message;
        shell_host_.Signatures().DeleteTemplate(selected->id, &error_message);
        shell_host_.Signatures().Discover(shell_host_.Signatures().RootDirectory(), &error_message);
        shell_host_.ReloadWorkspace();
        Refresh();
    }
};

class StationeryPane final : public EditorPaneBase {
public:
    explicit StationeryPane(HaikuShellHost& shell_host)
        : EditorPaneBase(shell_host, "stationery", "Stationery") {
        Refresh();
    }

protected:
    void PopulateItems() override {
        ConfigurePane("Stationery", true, false, true, true, true, "Refresh");
        for (const auto& stationery : shell_host_.Stationery().Templates()) {
            std::ostringstream detail;
            if (!stationery.headers.subject.empty()) {
                detail << "Subject: " << stationery.headers.subject << '\n';
            }
            if (!stationery.persona.empty()) {
                detail << "Persona: " << stationery.persona << '\n';
            }
            if (!stationery.signature_name.empty()) {
                detail << "Signature: " << stationery.signature_name << '\n';
            }
            detail << '\n' << stationery.body.plain_text;
            PushItem(stationery.name, stationery.name, detail.str());
        }
    }

    void SaveCurrent() override {
        StationeryTemplate template_entry;
        template_entry.name = CurrentName();
        template_entry.body.plain_text = CurrentDetail();
        std::string error_message;
        shell_host_.Stationery().SaveTemplate(template_entry, &error_message);
        shell_host_.Stationery().Discover(shell_host_.Stationery().RootDirectory(), &error_message);
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void DeleteCurrent() override {
        const auto selected = SelectedItem();
        if (!selected) {
            return;
        }
        std::string error_message;
        shell_host_.Stationery().DeleteTemplate(selected->id, &error_message);
        shell_host_.Stationery().Discover(shell_host_.Stationery().RootDirectory(), &error_message);
        shell_host_.ReloadWorkspace();
        Refresh();
    }
};

class NicknamesPane final : public EditorPaneBase {
public:
    explicit NicknamesPane(HaikuShellHost& shell_host)
        : EditorPaneBase(shell_host, "nicknames", "Nicknames") {
        Refresh();
    }

protected:
    void PopulateItems() override {
        ConfigurePane("Nicknames", true, false, true, true, true, "Refresh");
        for (const auto& nickname : shell_host_.Nicknames().Entries()) {
            std::ostringstream detail;
            detail << "FullName: " << nickname.full_name << '\n'
                   << "Addresses: " << JoinLines(nickname.addresses) << '\n'
                   << "RecipientList: " << (nickname.recipient_list ? "1" : "0") << '\n'
                   << "BPList: " << (nickname.bp_list ? "1" : "0") << '\n'
                   << "Notes: " << nickname.notes << '\n';
            PushItem(nickname.nickname, nickname.nickname, detail.str());
        }
    }

    void SaveCurrent() override {
        NicknameEntry nickname;
        nickname.nickname = CurrentName();
        nickname.addresses = {CurrentDetail()};
        shell_host_.Nicknames().AddOrReplace(nickname);
        std::string error_message;
        shell_host_.Nicknames().SaveToFile(shell_host_.DataRootPath() / "Nicknames.txt", &error_message);
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void DeleteCurrent() override {
        const auto selected = SelectedItem();
        if (!selected) {
            return;
        }
        shell_host_.Nicknames().Remove(selected->id);
        std::string error_message;
        shell_host_.Nicknames().SaveToFile(shell_host_.DataRootPath() / "Nicknames.txt", &error_message);
        shell_host_.ReloadWorkspace();
        Refresh();
    }
};

class PersonalitiesPane final : public WazooPaneBase {
public:
    explicit PersonalitiesPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "personalities") {
        account_list_ = new BListView("personalities-list");
        account_list_->SetSelectionMessage(new BMessage(kPersonaSelectionMessage));
        account_list_->SetInvocationMessage(new BMessage(kPersonaSelectionMessage));

        display_name_control_ = new BTextControl("persona-name", "Name", "", nullptr);
        email_control_ = new BTextControl("persona-email", "Email", "", nullptr);
        login_control_ = new BTextControl("persona-login", "Login", "", nullptr);
        reply_to_control_ = new BTextControl("persona-reply-to", "Reply-To", "", nullptr);
        incoming_server_control_ = new BTextControl("persona-in-server", "Incoming", "", nullptr);
        outgoing_server_control_ = new BTextControl("persona-out-server", "Outgoing", "", nullptr);
        incoming_port_control_ = new BTextControl("persona-in-port", "In Port", "", nullptr);
        outgoing_port_control_ = new BTextControl("persona-out-port", "Out Port", "", nullptr);
        incoming_password_control_ = new BTextControl("persona-in-pass", "In Password", "", nullptr);
        outgoing_password_control_ = new BTextControl("persona-out-pass", "Out Password", "", nullptr);
        oauth_client_id_control_ = new BTextControl("persona-oauth-client", "Client ID", "", nullptr);
        oauth_tenant_control_ = new BTextControl("persona-oauth-tenant", "Tenant/Domain", "", nullptr);
        oauth_scopes_control_ = new BTextControl("persona-oauth-scopes", "Scopes", "", nullptr);
        oauth_device_endpoint_control_ = new BTextControl("persona-oauth-device", "Device Endpoint", "", nullptr);
        oauth_token_endpoint_control_ = new BTextControl("persona-oauth-token", "Token Endpoint", "", nullptr);
        oauth_client_secret_control_ = new BTextControl("persona-oauth-secret", "Client Secret", "", nullptr);
        if (incoming_password_control_->TextView() != nullptr) {
            incoming_password_control_->TextView()->HideTyping(true);
        }
        if (outgoing_password_control_->TextView() != nullptr) {
            outgoing_password_control_->TextView()->HideTyping(true);
        }
        if (oauth_client_secret_control_->TextView() != nullptr) {
            oauth_client_secret_control_->TextView()->HideTyping(true);
        }

        protocol_menu_ = BuildMenuField("persona-protocol", "Protocol", {"IMAP", "POP"});
        incoming_security_menu_ =
            BuildMenuField("persona-in-security", "In Security", {"Plaintext", "Implicit TLS", "STARTTLS"});
        outgoing_security_menu_ =
            BuildMenuField("persona-out-security", "Out Security", {"Plaintext", "Implicit TLS", "STARTTLS"});
        incoming_auth_menu_ = BuildMenuField(
            "persona-in-auth", "In Auth", {"Password", "Kerberos", "CRAM-MD5", "APOP", "RPA", "OAuth 2.0"});
        outgoing_auth_menu_ = BuildMenuField(
            "persona-out-auth", "Out Auth", {"None", "CRAM-MD5", "LOGIN", "PLAIN", "OAuth 2.0"});
        oauth_provider_menu_ =
            BuildMenuField("persona-oauth-provider", "Provider", {"None", "Microsoft 365", "Google", "Custom"});
        oauth_mechanism_menu_ =
            BuildMenuField("persona-oauth-mechanism", "Mechanism", {"XOAUTH2", "OAUTHBEARER"});

        smtp_auth_allowed_box_ = new BCheckBox("persona-smtp-auth", "Allow SMTP authentication", nullptr);
        check_mail_by_default_box_ = new BCheckBox("persona-check-default", "Check mail by default", nullptr);
        client_secret_required_box_ =
            new BCheckBox("persona-client-secret-required", "Provider requires client secret", nullptr);

        status_view_ = new BTextView("persona-status");
        status_view_->MakeEditable(false);
        status_view_->SetWordWrap(true);
        status_view_->SetInsets(8, 8, 8, 8);

        auto* new_button = new BButton("persona-new", "New", new BMessage(kPersonaNewMessage));
        auto* save_button = new BButton("persona-save", "Save", new BMessage(kPersonaSaveMessage));
        auto* delete_button = new BButton("persona-delete", "Delete", new BMessage(kPersonaDeleteMessage));
        auto* authorize_button = new BButton("persona-authorize", "Authorize", new BMessage(kPersonaAuthorizeMessage));
        auto* reauthorize_button =
            new BButton("persona-reauthorize", "Re-authorize", new BMessage(kPersonaReauthorizeMessage));
        auto* forget_tokens_button =
            new BButton("persona-forget", "Forget Tokens", new BMessage(kPersonaForgetTokensMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(new BStringView("personalities-summary", "Personalities"))
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new BScrollView("personalities-list-scroll", account_list_, 0, false, true), 0.28f)
                .AddGroup(B_VERTICAL, 8, 0.72f)
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(display_name_control_)
                        .Add(email_control_)
                    .End()
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(login_control_)
                        .Add(reply_to_control_)
                    .End()
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(protocol_menu_)
                        .Add(incoming_auth_menu_)
                        .Add(outgoing_auth_menu_)
                    .End()
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(incoming_security_menu_)
                        .Add(outgoing_security_menu_)
                        .Add(smtp_auth_allowed_box_)
                    .End()
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(incoming_server_control_)
                        .Add(incoming_port_control_)
                    .End()
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(outgoing_server_control_)
                        .Add(outgoing_port_control_)
                    .End()
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(incoming_password_control_)
                        .Add(outgoing_password_control_)
                    .End()
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(oauth_provider_menu_)
                        .Add(oauth_mechanism_menu_)
                        .Add(check_mail_by_default_box_)
                    .End()
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(oauth_client_id_control_)
                        .Add(oauth_tenant_control_)
                    .End()
                    .Add(oauth_scopes_control_)
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(oauth_device_endpoint_control_)
                        .Add(oauth_token_endpoint_control_)
                    .End()
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(oauth_client_secret_control_)
                        .Add(client_secret_required_box_)
                    .End()
                    .Add(new BScrollView("persona-status-scroll", status_view_, 0, false, true), 1.0f)
                    .AddGroup(B_HORIZONTAL, 8)
                        .Add(new_button)
                        .Add(save_button)
                        .Add(delete_button)
                        .AddGlue()
                        .Add(authorize_button)
                        .Add(reauthorize_button)
                        .Add(forget_tokens_button)
                    .End()
                .End()
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        account_list_->SetTarget(this);
        for (BMenuField* field : {protocol_menu_,
                                  incoming_security_menu_,
                                  outgoing_security_menu_,
                                  incoming_auth_menu_,
                                  outgoing_auth_menu_,
                                  oauth_provider_menu_,
                                  oauth_mechanism_menu_}) {
            if (field != nullptr && field->Menu() != nullptr) {
                field->Menu()->SetTargetForItems(this);
            }
        }
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kPersonaSelectionMessage:
                LoadSelectedAccount();
                return;
            case kPersonaNewMessage:
                NewAccount();
                return;
            case kPersonaSaveMessage:
                SaveCurrent();
                return;
            case kPersonaDeleteMessage:
                DeleteCurrent();
                return;
            case kPersonaAuthorizeMessage:
                StartAuthorization(false);
                return;
            case kPersonaReauthorizeMessage:
                StartAuthorization(true);
                return;
            case kPersonaForgetTokensMessage:
                ForgetTokens();
                return;
            case kPersonaPollMessage:
                PollAuthorization();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        accounts_ = shell_host_.Accounts().Accounts();
        account_list_->MakeEmpty();
        int32 selected_index = -1;
        for (std::size_t index = 0; index < accounts_.size(); ++index) {
            const auto& account = accounts_[index];
            account_list_->AddItem(new BStringItem(account.display_name.empty() ? account.id.c_str()
                                                                                : account.display_name.c_str()));
            if (!current_account_id_.empty() && account.id == current_account_id_) {
                selected_index = static_cast<int32>(index);
            }
        }
        if (selected_index < 0 && !accounts_.empty()) {
            selected_index = 0;
        }
        if (selected_index >= 0) {
            account_list_->Select(selected_index);
            LoadSelectedAccount();
        } else {
            NewAccount();
        }
    }

private:
    void NewAccount() {
        current_account_id_.clear();
        pending_authorization_.reset();
        pending_account_id_.clear();
        poll_runner_.reset();
        display_name_control_->SetText("");
        email_control_->SetText("");
        login_control_->SetText("");
        reply_to_control_->SetText("");
        incoming_server_control_->SetText("");
        outgoing_server_control_->SetText("");
        incoming_port_control_->SetText("993");
        outgoing_port_control_->SetText("587");
        incoming_password_control_->SetText("");
        outgoing_password_control_->SetText("");
        oauth_client_id_control_->SetText("");
        oauth_tenant_control_->SetText("");
        oauth_scopes_control_->SetText("");
        oauth_device_endpoint_control_->SetText("");
        oauth_token_endpoint_control_->SetText("");
        oauth_client_secret_control_->SetText("");
        SetMarkedMenuValue(protocol_menu_, "IMAP");
        SetMarkedMenuValue(incoming_security_menu_, "Implicit TLS");
        SetMarkedMenuValue(outgoing_security_menu_, "STARTTLS");
        SetMarkedMenuValue(incoming_auth_menu_, "Password");
        SetMarkedMenuValue(outgoing_auth_menu_, "LOGIN");
        SetMarkedMenuValue(oauth_provider_menu_, "None");
        SetMarkedMenuValue(oauth_mechanism_menu_, "XOAUTH2");
        smtp_auth_allowed_box_->SetValue(B_CONTROL_ON);
        check_mail_by_default_box_->SetValue(B_CONTROL_ON);
        client_secret_required_box_->SetValue(B_CONTROL_OFF);
        account_list_->DeselectAll();
        UpdateStatusText("No OAuth authorization for this account.");
    }

    void LoadSelectedAccount() {
        const int32 index = account_list_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= accounts_.size()) {
            NewAccount();
            return;
        }
        const auto& account = accounts_[static_cast<std::size_t>(index)];
        current_account_id_ = account.id;
        display_name_control_->SetText(account.display_name.c_str());
        email_control_->SetText(account.email_address.c_str());
        login_control_->SetText(account.login_name.c_str());
        reply_to_control_->SetText(account.reply_to_address.c_str());
        incoming_server_control_->SetText(account.incoming_server.c_str());
        outgoing_server_control_->SetText(account.outgoing_server.c_str());
        incoming_port_control_->SetText(std::to_string(account.incoming_port).c_str());
        outgoing_port_control_->SetText(std::to_string(account.outgoing_port).c_str());
        incoming_password_control_->SetText(
            shell_host_.Credentials().LoadCredential(account.id, CredentialKind::kIncoming).value_or("").c_str());
        outgoing_password_control_->SetText(
            shell_host_.Credentials().LoadCredential(account.id, CredentialKind::kOutgoing).value_or("").c_str());
        oauth_client_secret_control_->SetText(
            shell_host_.Credentials().LoadCredential(account.id, CredentialKind::kOAuthClientSecret).value_or("").c_str());
        SetMarkedMenuValue(protocol_menu_, account.uses_imap ? "IMAP" : "POP");
        SetMarkedMenuValue(incoming_security_menu_, SecurityLabel(account.incoming_security));
        SetMarkedMenuValue(outgoing_security_menu_, SecurityLabel(account.outgoing_security));
        SetMarkedMenuValue(incoming_auth_menu_, IncomingAuthLabel(account));
        SetMarkedMenuValue(outgoing_auth_menu_, OutgoingAuthLabel(account.smtp_auth));
        SetMarkedMenuValue(oauth_provider_menu_, OAuthProviderLabel(account.oauth.provider_kind));
        SetMarkedMenuValue(oauth_mechanism_menu_, OAuthMechanismLabel(account.oauth.auth_mechanism));
        smtp_auth_allowed_box_->SetValue(account.smtp_auth_allowed ? B_CONTROL_ON : B_CONTROL_OFF);
        check_mail_by_default_box_->SetValue(account.check_mail_by_default ? B_CONTROL_ON : B_CONTROL_OFF);
        client_secret_required_box_->SetValue(account.oauth.client_secret_required ? B_CONTROL_ON : B_CONTROL_OFF);
        oauth_client_id_control_->SetText(account.oauth.client_id.c_str());
        oauth_tenant_control_->SetText(account.oauth.tenant_or_domain.c_str());
        oauth_scopes_control_->SetText(JoinLines(account.oauth.scopes).c_str());
        oauth_device_endpoint_control_->SetText(account.oauth.device_authorization_endpoint.c_str());
        oauth_token_endpoint_control_->SetText(account.oauth.token_endpoint.c_str());
        UpdateStatusFromAccount(account);
    }

    static std::uint16_t ParsePort(const char* text, std::uint16_t fallback) {
        if (text == nullptr || *text == '\0') {
            return fallback;
        }
        const int port = std::atoi(text);
        if (port <= 0 || port > 65535) {
            return fallback;
        }
        return static_cast<std::uint16_t>(port);
    }

    AccountProfile BuildAccount() const {
        AccountProfile account;
        account.id = current_account_id_.empty()
                         ? (email_control_->Text() != nullptr && std::strlen(email_control_->Text()) > 0
                                ? email_control_->Text()
                                : display_name_control_->Text())
                         : current_account_id_;
        account.display_name = display_name_control_->Text() != nullptr ? display_name_control_->Text() : "";
        account.email_address = email_control_->Text() != nullptr ? email_control_->Text() : "";
        account.login_name = login_control_->Text() != nullptr ? login_control_->Text() : "";
        account.reply_to_address = reply_to_control_->Text() != nullptr ? reply_to_control_->Text() : "";
        account.incoming_server = incoming_server_control_->Text() != nullptr ? incoming_server_control_->Text() : "";
        account.outgoing_server = outgoing_server_control_->Text() != nullptr ? outgoing_server_control_->Text() : "";
        account.incoming_port = ParsePort(incoming_port_control_->Text(), MenuValue(protocol_menu_) == "IMAP" ? 993 : 110);
        account.outgoing_port = ParsePort(outgoing_port_control_->Text(), 587);
        account.uses_imap = MenuValue(protocol_menu_) == "IMAP";
        account.uses_pop = !account.uses_imap;
        account.incoming_security = SecurityModeFromLabel(MenuValue(incoming_security_menu_));
        account.outgoing_security = SecurityModeFromLabel(MenuValue(outgoing_security_menu_));
        account.check_mail_by_default = check_mail_by_default_box_->Value() == B_CONTROL_ON;
        account.smtp_auth_allowed = smtp_auth_allowed_box_->Value() == B_CONTROL_ON;
        account.trash_mailbox_name = "Trash";

        const std::string incoming_auth = MenuValue(incoming_auth_menu_);
        if (account.uses_imap) {
            if (incoming_auth == "Kerberos") {
                account.imap_auth = ImapAuthMode::kKerberos;
            } else if (incoming_auth == "CRAM-MD5") {
                account.imap_auth = ImapAuthMode::kCramMd5;
            } else if (incoming_auth == "OAuth 2.0") {
                account.imap_auth = ImapAuthMode::kOAuth2;
            } else {
                account.imap_auth = ImapAuthMode::kPassword;
            }
        } else {
            if (incoming_auth == "Kerberos") {
                account.pop_auth = PopAuthMode::kKerberos;
            } else if (incoming_auth == "APOP") {
                account.pop_auth = PopAuthMode::kAPOP;
            } else if (incoming_auth == "RPA") {
                account.pop_auth = PopAuthMode::kRPA;
            } else if (incoming_auth == "OAuth 2.0") {
                account.pop_auth = PopAuthMode::kOAuth2;
            } else {
                account.pop_auth = PopAuthMode::kPassword;
            }
        }

        const std::string outgoing_auth = MenuValue(outgoing_auth_menu_);
        if (outgoing_auth == "CRAM-MD5") {
            account.smtp_auth = SmtpAuthMode::kCramMd5;
        } else if (outgoing_auth == "LOGIN") {
            account.smtp_auth = SmtpAuthMode::kLogin;
        } else if (outgoing_auth == "PLAIN") {
            account.smtp_auth = SmtpAuthMode::kPlain;
        } else if (outgoing_auth == "OAuth 2.0") {
            account.smtp_auth = SmtpAuthMode::kOAuth2;
        } else {
            account.smtp_auth = SmtpAuthMode::kNone;
        }

        const std::string provider = MenuValue(oauth_provider_menu_);
        if (provider == "Microsoft 365") {
            account.oauth.provider_kind = OAuthProviderKind::kMicrosoft365;
        } else if (provider == "Google") {
            account.oauth.provider_kind = OAuthProviderKind::kGoogle;
        } else if (provider == "Custom") {
            account.oauth.provider_kind = OAuthProviderKind::kCustom;
        } else {
            account.oauth.provider_kind = OAuthProviderKind::kNone;
        }
        account.oauth.auth_mechanism = MenuValue(oauth_mechanism_menu_) == "OAUTHBEARER"
                                           ? OAuthAuthMechanism::kOAUTHBEARER
                                           : OAuthAuthMechanism::kXOAUTH2;
        account.oauth.client_id = oauth_client_id_control_->Text() != nullptr ? oauth_client_id_control_->Text() : "";
        account.oauth.tenant_or_domain =
            oauth_tenant_control_->Text() != nullptr ? oauth_tenant_control_->Text() : "";
        account.oauth.device_authorization_endpoint =
            oauth_device_endpoint_control_->Text() != nullptr ? oauth_device_endpoint_control_->Text() : "";
        account.oauth.token_endpoint =
            oauth_token_endpoint_control_->Text() != nullptr ? oauth_token_endpoint_control_->Text() : "";
        account.oauth.client_secret_required = client_secret_required_box_->Value() == B_CONTROL_ON;
        account.oauth.scopes.clear();
        {
            std::istringstream stream(oauth_scopes_control_->Text() != nullptr ? oauth_scopes_control_->Text() : "");
            std::string scope;
            while (stream >> scope) {
                account.oauth.scopes.push_back(scope);
            }
        }
        account.kerberos.service_name = account.uses_imap ? "imap" : "pop";
        account.kerberos.service_format = "%s@%h";
        account.kerberos.pop_port = account.incoming_port;
        return account;
    }

    bool PersistAccount(std::string* saved_account_id = nullptr) {
        AccountProfile account = BuildAccount();
        if (account.id.empty()) {
            UpdateStatusText("Account name or email is required before saving.");
            return false;
        }

        shell_host_.Accounts().AddOrReplace(account);
        std::string error_message;
        if (!shell_host_.Accounts().SaveToIniFile(shell_host_.SettingsFilePath(), &error_message)) {
            UpdateStatusText(error_message);
            return false;
        }
        if (incoming_password_control_->Text() != nullptr && std::strlen(incoming_password_control_->Text()) > 0) {
            shell_host_.Credentials().SaveCredential(account.id,
                                                     CredentialKind::kIncoming,
                                                     incoming_password_control_->Text(),
                                                     nullptr);
        }
        if (outgoing_password_control_->Text() != nullptr && std::strlen(outgoing_password_control_->Text()) > 0) {
            shell_host_.Credentials().SaveCredential(account.id,
                                                     CredentialKind::kOutgoing,
                                                     outgoing_password_control_->Text(),
                                                     nullptr);
        }
        if (oauth_client_secret_control_->Text() != nullptr &&
            std::strlen(oauth_client_secret_control_->Text()) > 0) {
            shell_host_.Credentials().SaveCredential(account.id,
                                                     CredentialKind::kOAuthClientSecret,
                                                     oauth_client_secret_control_->Text(),
                                                     nullptr);
        }
        current_account_id_ = account.id;
        if (saved_account_id != nullptr) {
            *saved_account_id = account.id;
        }
        shell_host_.ReloadWorkspace();
        Refresh();
        return true;
    }

    void SaveCurrent() {
        PersistAccount(nullptr);
    }

    void DeleteCurrent() {
        if (current_account_id_.empty()) {
            return;
        }
        std::string error_message;
        shell_host_.OAuthService().ForgetTokens(current_account_id_, &error_message);
        shell_host_.Accounts().Remove(current_account_id_);
        shell_host_.Accounts().SaveToIniFile(shell_host_.SettingsFilePath(), &error_message);
        current_account_id_.clear();
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void StartAuthorization(bool reauthorize) {
        std::string saved_account_id;
        if (!PersistAccount(&saved_account_id)) {
            return;
        }
        const auto account = shell_host_.Accounts().FindById(saved_account_id);
        if (!account) {
            UpdateStatusText("Unable to load the saved account for OAuth authorization.");
            return;
        }

        if (reauthorize) {
            std::string ignored;
            shell_host_.OAuthService().ForgetTokens(saved_account_id, &ignored);
        }

        pending_authorization_.emplace();
        MailTaskErrorKind error_kind = MailTaskErrorKind::kUnknown;
        std::string error_message;
        if (!shell_host_.OAuthService().BeginAuthorization(*account,
                                                           &*pending_authorization_,
                                                           &error_kind,
                                                           &error_message)) {
            pending_authorization_.reset();
            UpdateStatusText(error_message);
            shell_host_.Tasks().UpsertTask({saved_account_id + "-oauth",
                                            account->display_name,
                                            "OAuth authorization",
                                            "Failed",
                                            error_message,
                                            MailTaskKind::kAuthentication,
                                            MailTaskState::kFailed,
                                            0,
                                            0,
                                            true});
            shell_host_.Tasks().FailTask(saved_account_id + "-oauth",
                                         "Failed",
                                         error_message,
                                         error_kind,
                                         OAuthMechanismLabel(account->oauth.auth_mechanism));
            return;
        }

        pending_account_id_ = saved_account_id;
        authorization_task_id_ = saved_account_id + "-oauth";
        MailTaskRecord task;
        task.id = authorization_task_id_;
        task.persona = account->display_name.empty() ? account->id : account->display_name;
        task.title = "OAuth authorization";
        task.status = "Waiting";
        task.details = pending_authorization_->verification_uri + " [" + pending_authorization_->user_code + "]";
        task.kind = MailTaskKind::kAuthentication;
        task.state = MailTaskState::kWaiting;
        task.allow_bring_to_front = true;
        shell_host_.Tasks().UpsertTask(task);

        const std::string browser_target =
            pending_authorization_->verification_uri_complete.empty()
                ? pending_authorization_->verification_uri
                : pending_authorization_->verification_uri_complete;
        if (!browser_target.empty()) {
            const std::string command = "open \"" + browser_target + "\" >/dev/null 2>&1";
            std::system(command.c_str());
        }

        poll_runner_ = std::make_unique<BMessageRunner>(BMessenger(this),
                                                        new BMessage(kPersonaPollMessage),
                                                        static_cast<bigtime_t>(pending_authorization_->interval_seconds) * 1000000LL,
                                                        -1);
        UpdateStatusFromAccount(*account);
    }

    void PollAuthorization() {
        if (!pending_authorization_ || pending_account_id_.empty()) {
            return;
        }
        const auto account = shell_host_.Accounts().FindById(pending_account_id_);
        if (!account) {
            poll_runner_.reset();
            pending_authorization_.reset();
            return;
        }

        int next_interval = pending_authorization_->interval_seconds;
        OAuthTokenRecord token_record;
        MailTaskErrorKind error_kind = MailTaskErrorKind::kUnknown;
        std::string error_message;
        const OAuthPollState state = shell_host_.OAuthService().PollAuthorization(*account,
                                                                                  *pending_authorization_,
                                                                                  &next_interval,
                                                                                  &token_record,
                                                                                  &error_kind,
                                                                                  &error_message);
        if (state == OAuthPollState::kSucceeded) {
            shell_host_.Tasks().CompleteTask(authorization_task_id_, "Authorized");
            poll_runner_.reset();
            pending_authorization_.reset();
            UpdateStatusFromAccount(*account);
            return;
        }
        if (state == OAuthPollState::kAuthorizationPending || state == OAuthPollState::kSlowDown) {
            pending_authorization_->interval_seconds = next_interval;
            MailTaskRecord task;
            task.id = authorization_task_id_;
            task.persona = account->display_name.empty() ? account->id : account->display_name;
            task.title = "OAuth authorization";
            task.status = state == OAuthPollState::kSlowDown ? "Slow down" : "Waiting";
            task.details = error_message;
            task.kind = MailTaskKind::kAuthentication;
            task.state = MailTaskState::kWaiting;
            task.allow_bring_to_front = true;
            shell_host_.Tasks().UpsertTask(task);
            poll_runner_ = std::make_unique<BMessageRunner>(BMessenger(this),
                                                            new BMessage(kPersonaPollMessage),
                                                            static_cast<bigtime_t>(next_interval) * 1000000LL,
                                                            -1);
            UpdateStatusFromAccount(*account);
            return;
        }

        shell_host_.Tasks().FailTask(authorization_task_id_,
                                     "Failed",
                                     error_message,
                                     error_kind,
                                     OAuthMechanismLabel(account->oauth.auth_mechanism));
        poll_runner_.reset();
        pending_authorization_.reset();
        UpdateStatusText(error_message);
    }

    void ForgetTokens() {
        if (current_account_id_.empty()) {
            return;
        }
        std::string error_message;
        if (!shell_host_.OAuthService().ForgetTokens(current_account_id_, &error_message)) {
            UpdateStatusText(error_message);
            return;
        }
        UpdateStatusText("OAuth tokens removed.");
    }

    void UpdateStatusFromAccount(const AccountProfile& account) {
        std::ostringstream status;
        status << "Provider: " << OAuthProviderLabel(account.oauth.provider_kind) << '\n'
               << "Incoming auth: " << IncomingAuthLabel(account) << '\n'
               << "Outgoing auth: " << OutgoingAuthLabel(account.smtp_auth) << '\n';
        const auto token_status = shell_host_.OAuthService().TokenStatus(account.id);
        if (token_status.authorized) {
            status << "OAuth status: Authorized";
            if (token_status.access_token_valid) {
                status << " (token ready)";
            }
            status << '\n';
        } else {
            status << "OAuth status: Not authorized\n";
        }
        if (pending_authorization_ && pending_account_id_ == account.id) {
            status << "Verification URL: " << pending_authorization_->verification_uri << '\n'
                   << "User code: " << pending_authorization_->user_code << '\n';
            if (!pending_authorization_->message.empty()) {
                status << pending_authorization_->message << '\n';
            }
        }
        UpdateStatusText(status.str());
    }

    void UpdateStatusText(std::string_view text) {
        status_view_->SetText(std::string(text).c_str());
    }

    BListView* account_list_ = nullptr;
    BTextControl* display_name_control_ = nullptr;
    BTextControl* email_control_ = nullptr;
    BTextControl* login_control_ = nullptr;
    BTextControl* reply_to_control_ = nullptr;
    BTextControl* incoming_server_control_ = nullptr;
    BTextControl* outgoing_server_control_ = nullptr;
    BTextControl* incoming_port_control_ = nullptr;
    BTextControl* outgoing_port_control_ = nullptr;
    BTextControl* incoming_password_control_ = nullptr;
    BTextControl* outgoing_password_control_ = nullptr;
    BTextControl* oauth_client_id_control_ = nullptr;
    BTextControl* oauth_tenant_control_ = nullptr;
    BTextControl* oauth_scopes_control_ = nullptr;
    BTextControl* oauth_device_endpoint_control_ = nullptr;
    BTextControl* oauth_token_endpoint_control_ = nullptr;
    BTextControl* oauth_client_secret_control_ = nullptr;
    BMenuField* protocol_menu_ = nullptr;
    BMenuField* incoming_security_menu_ = nullptr;
    BMenuField* outgoing_security_menu_ = nullptr;
    BMenuField* incoming_auth_menu_ = nullptr;
    BMenuField* outgoing_auth_menu_ = nullptr;
    BMenuField* oauth_provider_menu_ = nullptr;
    BMenuField* oauth_mechanism_menu_ = nullptr;
    BCheckBox* smtp_auth_allowed_box_ = nullptr;
    BCheckBox* check_mail_by_default_box_ = nullptr;
    BCheckBox* client_secret_required_box_ = nullptr;
    BTextView* status_view_ = nullptr;
    std::vector<AccountProfile> accounts_;
    std::string current_account_id_;
    std::string pending_account_id_;
    std::string authorization_task_id_;
    std::optional<OAuthDeviceAuthorization> pending_authorization_;
    std::unique_ptr<BMessageRunner> poll_runner_;
};

class FiltersPane final : public EditorPaneBase {
public:
    explicit FiltersPane(HaikuShellHost& shell_host)
        : EditorPaneBase(shell_host, "filters", "Filters") {
        Refresh();
    }

protected:
    void PopulateItems() override {
        ConfigurePane("Filters", true, false, true, true, true, "Refresh");
        for (const auto& rule : shell_host_.Filters().Rules()) {
            std::ostringstream detail;
            detail << "Field: "
                   << (rule.field == hermes::FilterField::kFrom
                           ? "from"
                           : rule.field == hermes::FilterField::kTo
                                 ? "to"
                                 : rule.field == hermes::FilterField::kBody ? "body" : "subject")
                   << '\n'
                   << "Operation: "
                   << (rule.operation == hermes::FilterOperation::kEquals
                           ? "equals"
                           : rule.operation == hermes::FilterOperation::kNotContains ? "not-contains"
                                                                                     : "contains")
                   << '\n'
                   << "Value: " << rule.value << '\n'
                   << "DestinationMailbox: "
                   << (rule.destination_mailbox ? *rule.destination_mailbox : "") << '\n';
            PushItem(rule.name, rule.name, detail.str());
        }
    }

    void SaveCurrent() override {
        FilterRule rule;
        rule.name = CurrentName();
        rule.field = FilterField::kSubject;
        rule.operation = FilterOperation::kContains;
        rule.value = CurrentDetail();
        shell_host_.Filters().AddOrReplace(rule);
        std::string error_message;
        shell_host_.Filters().SaveToFile(shell_host_.DataRootPath() / "Filters.ini", &error_message);
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void DeleteCurrent() override {
        const auto selected = SelectedItem();
        if (!selected) {
            return;
        }
        shell_host_.Filters().Remove(selected->id);
        std::string error_message;
        shell_host_.Filters().SaveToFile(shell_host_.DataRootPath() / "Filters.ini", &error_message);
        shell_host_.ReloadWorkspace();
        Refresh();
    }
};

class FilterReportPane final : public EditorPaneBase {
public:
    explicit FilterReportPane(HaikuShellHost& shell_host)
        : EditorPaneBase(shell_host, "filter-report", "Filter Report") {
        Refresh();
    }

protected:
    void PopulateItems() override {
        ConfigurePane("Filter Report", false, false, false, false, false, "Refresh");
        for (const auto& entry : shell_host_.FilterReport().Entries()) {
            std::ostringstream detail;
            detail << "Mailbox: " << entry.mailbox_name << '\n'
                   << "Sender: " << entry.sender << '\n'
                   << "Subject: " << entry.subject << '\n'
                   << "Matched Rules: " << JoinLines(entry.matched_rules);
            PushItem(entry.id, entry.subject.empty() ? entry.message_id : entry.subject, detail.str());
        }
    }
};

class LinkHistoryPane final : public EditorPaneBase {
public:
    explicit LinkHistoryPane(HaikuShellHost& shell_host)
        : EditorPaneBase(shell_host, "link-history", "Link History") {
        Refresh();
    }

protected:
    void PopulateItems() override {
        ConfigurePane("Link History", false, false, false, false, false, "Refresh");
        for (const auto& entry : shell_host_.LinkHistory().Entries()) {
            std::ostringstream detail;
            detail << "Target: " << entry.target << '\n'
                   << "Source: " << entry.source_context << '\n'
                   << "Launched: " << (entry.launched ? "yes" : "no");
            PushItem(entry.id, entry.title.empty() ? entry.target : entry.title, detail.str());
        }
    }
};

class FileBrowserPane final : public WazooPaneBase {
public:
    explicit FileBrowserPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "file-browser") {
        summary_view_ = new BStringView("file-browser-summary", "File Browser");
        list_view_ = new BOutlineListView("file-browser-list");
        list_view_->SetSelectionMessage(new BMessage(kPaneSelectionMessage));
        detail_view_ = new BTextView("file-browser-detail");
        detail_view_->MakeEditable(false);
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);
        auto* refresh_button = new BButton("file-browser-refresh", "Refresh", new BMessage(kPaneActionMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(summary_view_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new BScrollView("file-browser-scroll", list_view_, 0, false, true), 0.42f)
                .Add(new BScrollView("file-browser-detail-scroll", detail_view_, 0, false, true), 0.58f)
            .End()
            .AddGroup(B_HORIZONTAL, 8)
                .Add(refresh_button)
                .AddGlue()
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kPaneSelectionMessage:
                UpdateDetail();
                return;
            case kPaneActionMessage:
                Refresh();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        list_view_->MakeEmpty();
        paths_.clear();
        const auto root = shell_host_.DataRootPath();
        if (!std::filesystem::exists(root)) {
            detail_view_->SetText("Data root unavailable.");
            return;
        }

        paths_.push_back(root);
        list_view_->AddItem(new BStringItem(root.filename().string().c_str(), 0, false));
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            const auto relative = std::filesystem::relative(entry.path(), root);
            const int32 depth = std::max<int32>(0, static_cast<int32>(std::distance(relative.begin(), relative.end())) - 1);
            paths_.push_back(entry.path());
            list_view_->AddItem(new BStringItem(entry.path().filename().string().c_str(), depth + 1, false));
        }
        if (list_view_->CountItems() > 0) {
            list_view_->Select(0);
        }
        UpdateDetail();
    }

private:
    void UpdateDetail() {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= paths_.size()) {
            detail_view_->SetText("Select a path.");
            return;
        }
        detail_view_->SetText(paths_[static_cast<std::size_t>(index)].string().c_str());
    }

    BStringView* summary_view_ = nullptr;
    BOutlineListView* list_view_ = nullptr;
    BTextView* detail_view_ = nullptr;
    std::vector<std::filesystem::path> paths_;
};

#endif

class MailboxesNavigatorPane final : public WazooPaneBase {
public:
    explicit MailboxesNavigatorPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "mailboxes") {
        list_view_ = new ContextOutlineListView("mailboxes-tree",
                                                new BMessage(kPaneSelectionMessage),
                                                new BMessage(kPaneInvokeMessage),
                                                kPaneDeleteMessage,
                                                [this](BPoint where) { ShowContextMenu(where); });
        summary_view_ = new BStringView("mailboxes-status", "Select a mailbox.");
        auto* open_button = new BButton("mailboxes-open", "Open Mailbox", new BMessage(kPaneActionMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(new BScrollView("mailboxes-tree-scroll", list_view_, 0, false, true))
            .Add(summary_view_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(open_button)
                .AddGlue()
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kPaneSelectionMessage:
                UpdateSummary();
                return;
            case kMailboxOpenMessage:
            case kPaneInvokeMessage:
            case kPaneActionMessage:
                OpenSelected();
                return;
            case kPaneNewMessage:
                PromptForNewMailbox();
                return;
            case kPaneSaveMessage:
            case kMailboxRenameMessage:
                PromptForRenameSelected();
                return;
            case kPaneDeleteMessage:
                DeleteSelected();
                return;
            case kMailboxFindMessagesMessage:
                OpenFindMessages();
                return;
            case kMailboxRefreshMessage:
                RefreshSelectedMailbox();
                return;
            case kMailboxResyncMessage:
                ResyncSelectedMailbox();
                return;
            case kMailboxResyncTreeMessage:
                ResyncMailboxTree();
                return;
            case kMailboxAutoSyncMessage:
                ToggleAutoSyncSelectedMailbox();
                return;
            case kMailboxShowDeletedMessage:
                ToggleShowDeletedSelectedMailbox();
                return;
            case kMailboxEmptyTrashMessage:
                EmptySelectedMailbox();
                return;
            case kMailboxTrimJunkMessage:
                TrimSelectedMailbox();
                return;
            case kMailboxCreateConfirmedMessage: {
                const char* name = nullptr;
                if (message->FindString("name", &name) == B_OK && name != nullptr) {
                    CreateMailbox(name);
                }
                return;
            }
            case kMailboxRenameConfirmedMessage: {
                const char* name = nullptr;
                if (message->FindString("name", &name) == B_OK && name != nullptr) {
                    RenameSelected(name);
                }
                return;
            }
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        list_view_->MakeEmpty();
        rows_.clear();

        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        const auto recent_mailbox_ids = hermes::NormalizeRecentMailboxIds(
            preferences.recent_mailbox_ids,
            shell_host_.Mailboxes(),
            MaxRecentMailboxCount(shell_host_.Settings()));
        if (recent_mailbox_ids != preferences.recent_mailbox_ids) {
            preferences.recent_mailbox_ids = recent_mailbox_ids;
            ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
            std::string ignored;
            shell_host_.PersistSettings(&ignored);
        }

        auto mailboxes = shell_host_.Workspace().Mailboxes();
        std::stable_sort(mailboxes.begin(), mailboxes.end(), [](const MailboxSummary& left, const MailboxSummary& right) {
            if (left.account_id != right.account_id) {
                return left.account_id < right.account_id;
            }
            if (left.parent_id != right.parent_id) {
                return left.parent_id < right.parent_id;
            }
            if (left.system_mailbox != right.system_mailbox) {
                return left.system_mailbox && !right.system_mailbox;
            }
            return left.display_name < right.display_name;
        });

        std::map<std::string, std::string> parents;
        for (const auto& mailbox : mailboxes) {
            parents.emplace(mailbox.id, mailbox.parent_id);
        }

        const auto depth_for = [&parents](const std::string& mailbox_id) {
            int32 depth = 0;
            auto current = parents.find(mailbox_id);
            while (current != parents.end() && !current->second.empty()) {
                ++depth;
                if (current->second.rfind("account:", 0) == 0) {
                    break;
                }
                current = parents.find(current->second);
            }
            return depth;
        };

        std::map<std::string, MailboxSummary> summaries_by_id;
        for (const auto& mailbox : mailboxes) {
            summaries_by_id.emplace(mailbox.id, mailbox);
        }

        const int32 recent_insert_after = [&mailboxes]() {
            int32 after_system = -1;
            for (std::size_t index = 0; index < mailboxes.size(); ++index) {
                const auto& mailbox = mailboxes[index];
                if (mailbox.system_mailbox) {
                    after_system = static_cast<int32>(index);
                }
                if (mailbox.id == "trash" || EqualsCaseInsensitive(mailbox.display_name, "Trash")) {
                    return static_cast<int32>(index);
                }
            }
            return after_system;
        }();

        const auto add_recent_rows = [&]() {
            if (recent_mailbox_ids.empty()) {
                return;
            }
            bool added_header = false;
            for (const auto& mailbox_id : recent_mailbox_ids) {
                const auto summary = summaries_by_id.find(mailbox_id);
                if (summary == summaries_by_id.end()) {
                    continue;
                }
                if (!added_header) {
                    auto* header = new BStringItem("Recent", 0, false);
                    header->SetEnabled(false);
                    list_view_->AddItem(header);
                    rows_.push_back({"", "Recent"});
                    added_header = true;
                }
                AddMailboxRow(MailboxRowLabel(summary->second), 1, mailbox_id);
            }
        };

        int32 selected_index = -1;
        if (recent_insert_after < 0) {
            add_recent_rows();
        }
        for (std::size_t index = 0; index < mailboxes.size(); ++index) {
            const auto& mailbox = mailboxes[index];
            AddMailboxRow(MailboxRowLabel(mailbox), depth_for(mailbox.id), mailbox.id);
            if (mailbox.id == shell_host_.ActiveMailboxId()) {
                selected_index = static_cast<int32>(rows_.size() - 1);
            }
            if (static_cast<int32>(index) == recent_insert_after) {
                add_recent_rows();
            }
        }
        if (selected_index < 0) {
            selected_index = FirstSelectableRow();
        }
        if (selected_index >= 0) {
            list_view_->Select(selected_index);
        }
        UpdateSummary();
    }

    bool CanFind() const override {
        return std::any_of(rows_.begin(), rows_.end(), [](const auto& row) { return !row.mailbox_id.empty(); });
    }

    bool HandleCommand(uint32 command_id) override {
        switch (command_id) {
            case kMailboxOpenMessage:
            case kPaneActionMessage:
                OpenSelected();
                return true;
            case kPaneNewMessage:
                PromptForNewMailbox();
                return true;
            case kMailboxRenameMessage:
            case kPaneSaveMessage:
                PromptForRenameSelected();
                return true;
            case kPaneDeleteMessage:
                DeleteSelected();
                return true;
            case kMailboxFindMessagesMessage:
                OpenFindMessages();
                return true;
            case kMailboxRefreshMessage:
                RefreshSelectedMailbox();
                return true;
            case kMailboxResyncMessage:
                ResyncSelectedMailbox();
                return true;
            case kMailboxResyncTreeMessage:
                ResyncMailboxTree();
                return true;
            case kMailboxAutoSyncMessage:
                ToggleAutoSyncSelectedMailbox();
                return true;
            case kMailboxShowDeletedMessage:
                ToggleShowDeletedSelectedMailbox();
                return true;
            case kMailboxEmptyTrashMessage:
                EmptySelectedMailbox();
                return true;
            case kMailboxTrimJunkMessage:
                TrimSelectedMailbox();
                return true;
            default:
                return false;
        }
    }

    bool IsCommandEnabled(uint32 command_id) const override {
        const auto selected = SelectedMailbox();
        const bool has_selection = selected.has_value();
        const bool rename_or_delete = has_selection && !selected->system_mailbox;
        const bool imap_mailbox = has_selection && selected->protocol == MailboxProtocol::kImap;
        const bool imap_leaf_mailbox = imap_mailbox && selected->kind == MailboxKind::kMailbox;
        switch (command_id) {
            case kPaneNewMessage:
                return true;
            case kMailboxOpenMessage:
            case kPaneActionMessage:
                return has_selection;
            case kMailboxRenameMessage:
            case kPaneSaveMessage:
                return rename_or_delete;
            case kPaneDeleteMessage:
                return rename_or_delete;
            case kMailboxFindMessagesMessage:
                return has_selection;
            case kMailboxRefreshMessage:
                return has_selection;
            case kMailboxResyncMessage:
            case kMailboxResyncTreeMessage:
                return imap_mailbox;
            case kMailboxAutoSyncMessage:
                return imap_leaf_mailbox && !IsImapInbox(*selected);
            case kMailboxShowDeletedMessage:
                return imap_leaf_mailbox;
            case kMailboxEmptyTrashMessage:
                return has_selection && IsTrashMailbox(*selected);
            case kMailboxTrimJunkMessage:
                return has_selection && IsJunkMailbox(*selected);
            default:
                return false;
        }
    }

    bool HandleFind(std::string_view term, bool repeat) override {
        if (term.empty() || rows_.empty()) {
            return false;
        }
        const int32 current = list_view_->CurrentSelection();
        std::size_t start = 0;
        if (repeat && current >= 0) {
            start = static_cast<std::size_t>(current + 1);
        } else if (!repeat && current >= 0) {
            start = static_cast<std::size_t>(current);
        }
        for (std::size_t offset = 0; offset < rows_.size(); ++offset) {
            const std::size_t index = (start + offset) % rows_.size();
            if (rows_[index].mailbox_id.empty()) {
                continue;
            }
            if (FindCaseInsensitive(rows_[index].label, term)) {
                list_view_->Select(static_cast<int32>(index));
                list_view_->ScrollToSelection();
                UpdateSummary();
                list_view_->MakeFocus(true);
                return true;
            }
        }
        return false;
    }

    BView* PreferredFocusView() const override {
        return list_view_;
    }

private:
    struct MailboxRow {
        std::string mailbox_id;
        std::string label;
    };

    void AddMailboxRow(std::string label, int32 level, std::string mailbox_id) {
        list_view_->AddItem(new BStringItem(label.c_str(), level, false));
        rows_.push_back({std::move(mailbox_id), std::move(label)});
    }

    int32 FirstSelectableRow() const {
        for (std::size_t index = 0; index < rows_.size(); ++index) {
            if (!rows_[index].mailbox_id.empty()) {
                return static_cast<int32>(index);
            }
        }
        return -1;
    }

    std::optional<MailboxRecord> SelectedMailbox() const {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= rows_.size()) {
            return std::nullopt;
        }
        const auto& mailbox_id = rows_[static_cast<std::size_t>(index)].mailbox_id;
        if (mailbox_id.empty()) {
            return std::nullopt;
        }
        return shell_host_.Mailboxes().GetMailbox(mailbox_id);
    }

    static bool IsTrashMailbox(const MailboxRecord& mailbox) {
        return mailbox.id == "trash" || EqualsCaseInsensitive(mailbox.display_name, "Trash");
    }

    static bool IsJunkMailbox(const MailboxRecord& mailbox) {
        return mailbox.id == "junk" || EqualsCaseInsensitive(mailbox.display_name, "Junk");
    }

    static bool IsImapInbox(const MailboxRecord& mailbox) {
        return mailbox.protocol == MailboxProtocol::kImap &&
               (EqualsCaseInsensitive(mailbox.remote_name, "INBOX") ||
                EqualsCaseInsensitive(mailbox.display_name, "Inbox"));
    }

    void UpdateSummary() {
        const int32 index = list_view_->CurrentSelection();
        if (index >= 0 && static_cast<std::size_t>(index) < rows_.size() &&
            rows_[static_cast<std::size_t>(index)].mailbox_id.empty()) {
            summary_view_->SetText("Recent mailboxes.");
            return;
        }
        const auto selected = SelectedMailbox();
        if (!selected) {
            summary_view_->SetText("Select a mailbox.");
            return;
        }
        std::ostringstream summary;
        summary << selected->display_name << "  |  "
                << (selected->protocol == MailboxProtocol::kImap
                        ? "IMAP"
                        : selected->protocol == MailboxProtocol::kPop ? "POP"
                                                                      : "Local")
                << "  |  " << (selected->system_mailbox ? "system" : "user");
        if (selected->is_remote && !selected->remote_name.empty()) {
            summary << "  |  " << selected->remote_name;
        }
        if (selected->protocol == MailboxProtocol::kImap && selected->kind == MailboxKind::kMailbox) {
            summary << "  |  auto-sync "
                    << (shell_host_.MailboxAutoSyncEnabled(selected->id) ? "on" : "off");
            summary << "  |  deleted "
                    << (shell_host_.MailboxShowsDeleted(selected->id) ? "shown" : "hidden");
        }
        summary_view_->SetText(summary.str().c_str());
    }

    void OpenSelected() {
        const auto selected = SelectedMailbox();
        if (!selected) {
            return;
        }
        shell_host_.OpenMailbox(selected->id);
        shell_host_.ShowMainWindow();
    }

    void OpenFindMessages() {
        const auto selected = SelectedMailbox();
        if (!selected) {
            return;
        }
        shell_host_.QueuePendingSearch({"",
                                        HaikuShellHost::SearchRequest::Scope::kCurrentMailbox,
                                        selected->id});
        shell_host_.OpenToolWindow("search");
    }

    void PromptForNewMailbox() {
        auto* prompt = new TextPromptWindow("New Mailbox",
                                            "Mailbox name",
                                            "",
                                            "name",
                                            BMessenger(this),
                                            BMessage(kMailboxCreateConfirmedMessage));
        prompt->Show();
    }

    void PromptForRenameSelected() {
        const auto selected = SelectedMailbox();
        if (!selected || selected->system_mailbox) {
            return;
        }
        auto* prompt = new TextPromptWindow("Rename Mailbox",
                                            "Mailbox name",
                                            selected->display_name,
                                            "name",
                                            BMessenger(this),
                                            BMessage(kMailboxRenameConfirmedMessage));
        prompt->Show();
    }

    void CreateMailbox(std::string_view raw_name) {
        const std::string display_name = TrimWhitespace(raw_name);
        if (display_name.empty()) {
            return;
        }
        const auto selected = SelectedMailbox();
        if (selected && selected->protocol == MailboxProtocol::kImap && selected->is_remote) {
            shell_host_.CreateRemoteMailbox(selected->account_id, display_name);
            return;
        }

        MailboxRecord record;
        record.id = SlugifyMailboxId(display_name);
        record.display_name = display_name;
        record.protocol = selected ? selected->protocol : MailboxProtocol::kLocal;
        if (record.protocol != MailboxProtocol::kImap) {
            record.protocol = MailboxProtocol::kLocal;
        }
        record.account_id = selected ? selected->account_id : "";
        record.parent_id = selected && selected->kind == MailboxKind::kFolder ? selected->id
                                                                               : selected ? selected->parent_id : "";
        std::string error_message;
        if (shell_host_.Mailboxes().EnsureMailbox(record, &error_message)) {
            shell_host_.ReloadWorkspace();
        }
    }

    void RenameSelected(std::string_view raw_name) {
        const auto selected = SelectedMailbox();
        if (!selected || selected->system_mailbox) {
            return;
        }
        const std::string display_name = TrimWhitespace(raw_name);
        if (display_name.empty()) {
            return;
        }
        if (selected->protocol == MailboxProtocol::kImap && selected->is_remote) {
            shell_host_.RenameRemoteMailbox(selected->id, display_name);
            return;
        }
        std::string error_message;
        if (shell_host_.Mailboxes().RenameMailbox(selected->id,
                                                  SlugifyMailboxId(display_name),
                                                  display_name,
                                                  &error_message)) {
            shell_host_.ReloadWorkspace();
        }
    }

    void DeleteSelected() {
        const auto selected = SelectedMailbox();
        if (!selected || selected->system_mailbox) {
            return;
        }
        if (BAlert("mailboxes-delete",
                   ("Delete mailbox \"" + selected->display_name + "\"?").c_str(),
                   "Cancel",
                   "Delete")
                ->Go() != 1) {
            return;
        }
        if (selected->protocol == MailboxProtocol::kImap && selected->is_remote) {
            shell_host_.DeleteRemoteMailbox(selected->id);
            return;
        }
        std::string error_message;
        if (shell_host_.Mailboxes().DeleteMailbox(selected->id, &error_message)) {
            shell_host_.ReloadWorkspace();
        }
    }

    void EmptySelectedMailbox() {
        const auto selected = SelectedMailbox();
        if (!selected || !IsTrashMailbox(*selected)) {
            return;
        }
        for (const auto& message : shell_host_.Messages().ListMessages(selected->id)) {
            std::string ignored;
            shell_host_.Messages().DeleteMessage(selected->id, message.id, &ignored);
        }
        shell_host_.ReloadWorkspace();
    }

    void TrimSelectedMailbox() {
        const auto selected = SelectedMailbox();
        if (!selected || !IsJunkMailbox(*selected)) {
            return;
        }
        for (const auto& message : shell_host_.Messages().ListMessages(selected->id)) {
            std::string ignored;
            shell_host_.Messages().DeleteMessage(selected->id, message.id, &ignored);
        }
        shell_host_.ReloadWorkspace();
    }

    void RefreshSelectedMailbox() {
        const auto selected = SelectedMailbox();
        if (!selected) {
            return;
        }
        if (selected->protocol == MailboxProtocol::kImap) {
            shell_host_.RefreshMailbox(selected->id);
        } else {
            shell_host_.ReloadWorkspace();
        }
    }

    void ResyncSelectedMailbox() {
        const auto selected = SelectedMailbox();
        if (selected && selected->protocol == MailboxProtocol::kImap) {
            shell_host_.ResyncMailbox(selected->id);
        }
    }

    void ResyncMailboxTree() {
        const auto selected = SelectedMailbox();
        if (!selected || selected->protocol != MailboxProtocol::kImap) {
            return;
        }
        shell_host_.ResyncMailboxTree(selected->id);
    }

    void ToggleAutoSyncSelectedMailbox() {
        const auto selected = SelectedMailbox();
        if (!selected || selected->protocol != MailboxProtocol::kImap ||
            selected->kind != MailboxKind::kMailbox || IsImapInbox(*selected)) {
            return;
        }
        shell_host_.SetMailboxAutoSyncEnabled(
            selected->id, !shell_host_.MailboxAutoSyncEnabled(selected->id));
        UpdateSummary();
    }

    void ToggleShowDeletedSelectedMailbox() {
        const auto selected = SelectedMailbox();
        if (!selected || selected->protocol != MailboxProtocol::kImap ||
            selected->kind != MailboxKind::kMailbox) {
            return;
        }
        shell_host_.SetMailboxShowsDeleted(
            selected->id, !shell_host_.MailboxShowsDeleted(selected->id));
        UpdateSummary();
    }

    void ShowContextMenu(BPoint where) {
        BPopUpMenu menu("mailboxes-context", false, false);
        auto* open_item = new BMenuItem("Open", new BMessage(kMailboxOpenMessage));
        auto* find_messages_item =
            new BMenuItem("Find Messages" B_UTF8_ELLIPSIS, new BMessage(kMailboxFindMessagesMessage));
        auto* rename_item = new BMenuItem("Rename", new BMessage(kMailboxRenameMessage));
        auto* delete_item = new BMenuItem("Delete", new BMessage(kPaneDeleteMessage));
        auto* empty_trash_item =
            new BMenuItem("Empty Trash", new BMessage(kMailboxEmptyTrashMessage));
        auto* trim_junk_item = new BMenuItem("Delete Old Junk", new BMessage(kMailboxTrimJunkMessage));
        auto* refresh_item = new BMenuItem("Refresh", new BMessage(kMailboxRefreshMessage));
        auto* resync_item = new BMenuItem("Resync", new BMessage(kMailboxResyncMessage));
        auto* resync_tree_item =
            new BMenuItem("Resync Tree", new BMessage(kMailboxResyncTreeMessage));
        auto* auto_sync_item = new BMenuItem("Auto-Sync", new BMessage(kMailboxAutoSyncMessage));
        auto* show_deleted_item =
            new BMenuItem("Show Deleted", new BMessage(kMailboxShowDeletedMessage));
        open_item->SetEnabled(IsCommandEnabled(kMailboxOpenMessage));
        find_messages_item->SetEnabled(IsCommandEnabled(kMailboxFindMessagesMessage));
        rename_item->SetEnabled(IsCommandEnabled(kMailboxRenameMessage));
        delete_item->SetEnabled(IsCommandEnabled(kPaneDeleteMessage));
        empty_trash_item->SetEnabled(IsCommandEnabled(kMailboxEmptyTrashMessage));
        trim_junk_item->SetEnabled(IsCommandEnabled(kMailboxTrimJunkMessage));
        refresh_item->SetEnabled(IsCommandEnabled(kMailboxRefreshMessage));
        resync_item->SetEnabled(IsCommandEnabled(kMailboxResyncMessage));
        resync_tree_item->SetEnabled(IsCommandEnabled(kMailboxResyncTreeMessage));
        auto_sync_item->SetEnabled(IsCommandEnabled(kMailboxAutoSyncMessage));
        show_deleted_item->SetEnabled(IsCommandEnabled(kMailboxShowDeletedMessage));
        if (const auto selected = SelectedMailbox()) {
            auto_sync_item->SetMarked(shell_host_.MailboxAutoSyncEnabled(selected->id));
            show_deleted_item->SetMarked(shell_host_.MailboxShowsDeleted(selected->id));
        }
        menu.AddItem(open_item);
        menu.AddItem(find_messages_item);
        menu.AddSeparatorItem();
        menu.AddItem(new BMenuItem("New Mailbox", new BMessage(kPaneNewMessage)));
        menu.AddItem(rename_item);
        menu.AddItem(delete_item);
        menu.AddSeparatorItem();
        menu.AddItem(empty_trash_item);
        menu.AddItem(trim_junk_item);
        menu.AddSeparatorItem();
        menu.AddItem(refresh_item);
        menu.AddItem(resync_item);
        menu.AddItem(resync_tree_item);
        menu.AddItem(auto_sync_item);
        menu.AddItem(show_deleted_item);
        menu.SetTargetForItems(this);
        list_view_->ConvertToScreen(&where);
        menu.Go(where, true, false, true);
    }

    ContextOutlineListView* list_view_ = nullptr;
    BStringView* summary_view_ = nullptr;
    std::vector<MailboxRow> rows_;
};

class NicknameManagerPane final : public WazooPaneBase {
public:
    explicit NicknameManagerPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "nicknames") {
        list_view_ = new ContextListView("nicknames-list",
                                         new BMessage(kNicknameSelectionMessage),
                                         new BMessage(kNicknameComposeMessage),
                                         kNicknameDeleteMessage,
                                         [this](BPoint where) { ShowContextMenu(where); });
        split_view_ = new BSplitView(B_HORIZONTAL);

        nickname_control_ = new BTextControl("nickname-name", "Nickname", "", nullptr);
        full_name_control_ = new BTextControl("nickname-full-name", "Full Name", "", nullptr);
        addresses_view_ = new BTextView("nickname-addresses");
        addresses_view_->SetInsets(8, 8, 8, 8);
        notes_view_ = new BTextView("nickname-notes");
        notes_view_->SetInsets(8, 8, 8, 8);
        recipient_box_ = new BCheckBox("nickname-recipient-list", "Include in recipient list", nullptr);
        bp_box_ = new BCheckBox("nickname-bp-list", "Include in Boss Protector list", nullptr);

        tabs_ = new BTabView("nicknames-tabs");
        auto* addresses_tab = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(addresses_tab, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(new BStringView("nickname-addresses-help", "One address per line"))
            .Add(new BScrollView("nickname-addresses-scroll", addresses_view_, 0, false, true));
        tabs_->AddTab(addresses_tab);
        if (BTab* tab = tabs_->TabAt(0)) {
            tab->SetLabel("Address");
        }

        auto* notes_tab = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(notes_tab, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(new BScrollView("nickname-notes-scroll", notes_view_, 0, false, true));
        tabs_->AddTab(notes_tab);
        if (BTab* tab = tabs_->TabAt(1)) {
            tab->SetLabel("Notes");
        }

        auto* options_tab = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(options_tab, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(recipient_box_)
            .Add(bp_box_)
            .AddGlue();
        tabs_->AddTab(options_tab);
        if (BTab* tab = tabs_->TabAt(2)) {
            tab->SetLabel("Options");
        }

        editor_group_ = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(editor_group_, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(nickname_control_)
            .Add(full_name_control_)
            .Add(tabs_);

        split_view_->AddChild(new BScrollView("nicknames-list-scroll", list_view_, 0, false, true), 0.30f);
        split_view_->AddChild(editor_group_, 0.70f);

        auto* new_button = new BButton("nickname-new", "New", new BMessage(kNicknameNewMessage));
        auto* duplicate_button =
            new BButton("nickname-duplicate", "Duplicate", new BMessage(kNicknameDuplicateMessage));
        auto* save_button = new BButton("nickname-save", "Save", new BMessage(kNicknameSaveMessage));
        auto* delete_button = new BButton("nickname-delete", "Delete", new BMessage(kNicknameDeleteMessage));
        auto* compose_button =
            new BButton("nickname-compose", "New Message To", new BMessage(kNicknameComposeMessage));
        auto* details_button =
            new BButton("nickname-details", "Toggle Details", new BMessage(kWazooToggleDetailsMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(split_view_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new_button)
                .Add(duplicate_button)
                .Add(save_button)
                .Add(delete_button)
                .Add(details_button)
                .AddGlue()
                .Add(compose_button)
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
        preferred_focus_view_ = list_view_;
    }

    void PersistState() override {
        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        preferences.nicknames_split_layout =
            details_visible_ ? SerializeSplitWeights(*split_view_) : remembered_split_layout_;
        preferences.nicknames_rhs_visible = details_visible_;
        preferences.nicknames_selected_tab = tabs_ != nullptr ? tabs_->Selection() : 0;
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kNicknameSelectionMessage:
                HandleSelectionChange();
                return;
            case kNicknameNewMessage:
                HandleCommand(kNicknameNewMessage);
                return;
            case kNicknameDuplicateMessage:
                HandleCommand(kNicknameDuplicateMessage);
                return;
            case kNicknameSaveMessage:
                HandleCommand(kNicknameSaveMessage);
                return;
            case kNicknameDeleteMessage:
                HandleCommand(kNicknameDeleteMessage);
                return;
            case kNicknameComposeMessage:
                HandleCommand(kNicknameComposeMessage);
                return;
            case kWazooToggleDetailsMessage:
                HandleCommand(kWazooToggleDetailsMessage);
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        entries_ = shell_host_.Nicknames().Entries();
        std::stable_sort(entries_.begin(), entries_.end(), [](const auto& left, const auto& right) {
            return left.nickname < right.nickname;
        });
        list_view_->MakeEmpty();
        int32 selected_index = -1;
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            list_view_->AddItem(new BStringItem(entries_[index].nickname.c_str()));
            if (!current_nickname_.empty() && entries_[index].nickname == current_nickname_) {
                selected_index = static_cast<int32>(index);
            }
        }
        if (selected_index < 0 && !entries_.empty()) {
            selected_index = 0;
        }
        if (selected_index >= 0) {
            SelectEntryAt(selected_index);
        } else {
            ClearEditor();
        }
        ApplyStoredLayout();
    }

    bool HandleCommand(uint32 command_id) override {
        switch (command_id) {
            case kPaneNewMessage:
            case kNicknameNewMessage:
                if (!ResolveDirtyState("creating a new nickname")) {
                    return false;
                }
                ClearEditor();
                preferred_focus_view_ = nickname_control_->TextView();
                if (preferred_focus_view_ != nullptr) {
                    preferred_focus_view_->MakeFocus(true);
                }
                return true;
            case kNicknameDuplicateMessage:
                if (!ResolveDirtyState("duplicating this nickname")) {
                    return false;
                }
                DuplicateSelected();
                return true;
            case kPaneSaveMessage:
            case kNicknameSaveMessage:
                return SaveCurrent();
            case kPaneDeleteMessage:
            case kNicknameDeleteMessage:
                if (!ResolveDirtyState("deleting this nickname")) {
                    return false;
                }
                DeleteSelected();
                return true;
            case kNicknameComposeMessage:
                ComposeToSelected();
                return true;
            case kWazooToggleDetailsMessage:
                SetDetailsVisible(!details_visible_);
                PersistState();
                return true;
            default:
                return false;
        }
    }

    bool IsCommandEnabled(uint32 command_id) const override {
        switch (command_id) {
            case kPaneNewMessage:
            case kNicknameNewMessage:
                return true;
            case kNicknameDuplicateMessage:
            case kNicknameDeleteMessage:
            case kNicknameComposeMessage:
                return SelectedEntry().has_value();
            case kPaneDeleteMessage:
                return SelectedEntry().has_value();
            case kPaneSaveMessage:
            case kNicknameSaveMessage:
                return !BuildEditorEntry().nickname.empty() && IsDirty();
            case kWazooToggleDetailsMessage:
                return true;
            default:
                return false;
        }
    }

    bool CanFind() const override {
        return !entries_.empty();
    }

    bool HandleFind(std::string_view term, bool repeat) override {
        const auto match = FindMatch(term, repeat);
        if (!match) {
            return false;
        }
        ApplyMatch(*match);
        last_find_term_ = std::string(term);
        last_find_result_ = *match;
        return true;
    }

    BView* PreferredFocusView() const override {
        return preferred_focus_view_ != nullptr ? preferred_focus_view_ : list_view_;
    }

    bool CanDeactivate() override {
        return ResolveDirtyState("switching away from Nicknames");
    }

private:
    enum class MatchField {
        kNickname = 0,
        kFullName,
        kAddresses,
        kNotes,
        kRecipientList,
        kBossProtector,
    };

    struct MatchResult {
        std::size_t entry_index = 0;
        MatchField field = MatchField::kNickname;
        int32 start = 0;
        int32 end = 0;
    };

    static std::vector<std::string> SplitLines(std::string_view text) {
        std::vector<std::string> lines;
        std::istringstream stream(std::string(text));
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        return lines;
    }

    static std::string JoinAddresses(const std::vector<std::string>& addresses) {
        std::ostringstream stream;
        for (std::size_t index = 0; index < addresses.size(); ++index) {
            if (index != 0) {
                stream << ", ";
            }
            stream << addresses[index];
        }
        return stream.str();
    }

    std::optional<NicknameEntry> SelectedEntry() const {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= entries_.size()) {
            return std::nullopt;
        }
        return entries_[static_cast<std::size_t>(index)];
    }

    NicknameEntry BuildEditorEntry() const {
        NicknameEntry nickname;
        nickname.nickname = nickname_control_->Text() != nullptr ? nickname_control_->Text() : "";
        nickname.full_name = full_name_control_->Text() != nullptr ? full_name_control_->Text() : "";
        nickname.addresses = SplitLines(addresses_view_->Text() != nullptr ? addresses_view_->Text() : "");
        nickname.notes = notes_view_->Text() != nullptr ? notes_view_->Text() : "";
        nickname.recipient_list = recipient_box_->Value() == B_CONTROL_ON;
        nickname.bp_list = bp_box_->Value() == B_CONTROL_ON;
        return nickname;
    }

    bool IsDirty() const {
        return BuildEditorEntry().nickname != loaded_entry_.nickname ||
               BuildEditorEntry().full_name != loaded_entry_.full_name ||
               BuildEditorEntry().addresses != loaded_entry_.addresses ||
               BuildEditorEntry().notes != loaded_entry_.notes ||
               BuildEditorEntry().recipient_list != loaded_entry_.recipient_list ||
               BuildEditorEntry().bp_list != loaded_entry_.bp_list;
    }

    void ClearEditor() {
        current_nickname_.clear();
        loaded_entry_ = NicknameEntry{};
        nickname_control_->SetText("");
        full_name_control_->SetText("");
        addresses_view_->SetText("");
        notes_view_->SetText("");
        recipient_box_->SetValue(B_CONTROL_OFF);
        bp_box_->SetValue(B_CONTROL_OFF);
        tabs_->Select(0);
        suppress_selection_message_ = true;
        list_view_->DeselectAll();
        suppress_selection_message_ = false;
        preferred_focus_view_ = list_view_;
    }

    void LoadEntry(const NicknameEntry& entry) {
        current_nickname_ = entry.nickname;
        loaded_entry_ = entry;
        nickname_control_->SetText(entry.nickname.c_str());
        full_name_control_->SetText(entry.full_name.c_str());
        addresses_view_->SetText(JoinLines(entry.addresses).c_str());
        notes_view_->SetText(entry.notes.c_str());
        recipient_box_->SetValue(entry.recipient_list ? B_CONTROL_ON : B_CONTROL_OFF);
        bp_box_->SetValue(entry.bp_list ? B_CONTROL_ON : B_CONTROL_OFF);
        preferred_focus_view_ = list_view_;
    }

    void LoadSelected() {
        const auto selected = SelectedEntry();
        if (!selected) {
            ClearEditor();
            return;
        }
        LoadEntry(*selected);
    }

    bool SaveCurrent() {
        NicknameEntry nickname = BuildEditorEntry();
        if (nickname.nickname.empty()) {
            return false;
        }

        shell_host_.Nicknames().AddOrReplace(nickname);
        if (!current_nickname_.empty() && current_nickname_ != nickname.nickname) {
            shell_host_.Nicknames().Remove(current_nickname_);
        }
        std::string error_message;
        shell_host_.Nicknames().SaveToFile(shell_host_.DataRootPath() / "Nicknames.txt", &error_message);
        current_nickname_ = nickname.nickname;
        loaded_entry_ = nickname;
        shell_host_.ReloadWorkspace();
        Refresh();
        return true;
    }

    void DeleteSelected() {
        const auto selected = SelectedEntry();
        if (!selected) {
            return;
        }
        shell_host_.Nicknames().Remove(selected->nickname);
        std::string error_message;
        shell_host_.Nicknames().SaveToFile(shell_host_.DataRootPath() / "Nicknames.txt", &error_message);
        current_nickname_.clear();
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void DuplicateSelected() {
        const auto selected = SelectedEntry();
        if (!selected) {
            return;
        }
        NicknameEntry duplicate = *selected;
        duplicate.nickname += " Copy";
        shell_host_.Nicknames().AddOrReplace(duplicate);
        std::string error_message;
        shell_host_.Nicknames().SaveToFile(shell_host_.DataRootPath() / "Nicknames.txt", &error_message);
        current_nickname_ = duplicate.nickname;
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void ApplyStoredLayout() {
        const auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        remembered_split_layout_ = preferences.nicknames_split_layout;
        tabs_->Select(std::clamp(preferences.nicknames_selected_tab, 0, 2));
        if (preferences.nicknames_rhs_visible) {
            details_visible_ = true;
            RestoreSplitWeights(*split_view_, remembered_split_layout_);
        } else {
            RestoreSplitWeights(*split_view_, remembered_split_layout_);
            details_visible_ = true;
            SetDetailsVisible(false);
        }
    }

    void SetDetailsVisible(bool visible) {
        if (visible == details_visible_) {
            return;
        }
        if (!visible) {
            remembered_split_layout_ = SerializeSplitWeights(*split_view_);
            split_view_->SetItemWeight(0, 1.0f, false);
            split_view_->SetItemWeight(1, 0.0f, false);
            preferred_focus_view_ = list_view_;
        } else {
            RestoreSplitWeights(*split_view_, remembered_split_layout_);
        }
        details_visible_ = visible;
        split_view_->InvalidateLayout();
        if (visible && preferred_focus_view_ != nullptr) {
            preferred_focus_view_->MakeFocus(true);
        }
    }

    bool ResolveDirtyState(std::string_view action_label) {
        if (!IsDirty()) {
            return true;
        }
        const auto choice = PromptToSaveChanges(
            "Nicknames",
            std::string("Save changes before ").append(action_label).append("?"));
        if (choice == DirtyPromptAction::kSave) {
            return SaveCurrent();
        }
        if (choice == DirtyPromptAction::kDiscard) {
            if (current_nickname_.empty()) {
                ClearEditor();
            } else {
                const auto selected = SelectedEntry();
                if (selected && selected->nickname == current_nickname_) {
                    LoadSelected();
                } else {
                    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const auto& entry) {
                        return entry.nickname == current_nickname_;
                    });
                    if (it != entries_.end()) {
                        LoadEntry(*it);
                    } else {
                        ClearEditor();
                    }
                }
            }
            return true;
        }
        return false;
    }

    void SelectEntryAt(int32 index) {
        if (index < 0 || static_cast<std::size_t>(index) >= entries_.size()) {
            return;
        }
        suppress_selection_message_ = true;
        list_view_->Select(index);
        suppress_selection_message_ = false;
        LoadSelected();
    }

    void RestoreSelectionToCurrent() {
        int32 selected_index = -1;
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            if (entries_[index].nickname == current_nickname_) {
                selected_index = static_cast<int32>(index);
                break;
            }
        }
        suppress_selection_message_ = true;
        if (selected_index >= 0) {
            list_view_->Select(selected_index);
        } else {
            list_view_->DeselectAll();
        }
        suppress_selection_message_ = false;
    }

    void HandleSelectionChange() {
        if (suppress_selection_message_) {
            return;
        }
        const auto selected = SelectedEntry();
        const std::string next_name = selected ? selected->nickname : std::string();
        if (next_name == current_nickname_) {
            LoadSelected();
            return;
        }
        if (!ResolveDirtyState("switching nicknames")) {
            RestoreSelectionToCurrent();
            return;
        }
        LoadSelected();
    }

    std::optional<MatchResult> FindMatch(std::string_view term, bool repeat) const {
        if (term.empty() || entries_.empty()) {
            return std::nullopt;
        }

        std::vector<MatchResult> matches;
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            const auto& entry = entries_[index];
            if (const auto offset = FindCaseInsensitive(entry.nickname, term)) {
                matches.push_back({index,
                                   MatchField::kNickname,
                                   *offset,
                                   static_cast<int32>(*offset + term.size())});
            }
            if (const auto offset = FindCaseInsensitive(entry.full_name, term)) {
                matches.push_back({index,
                                   MatchField::kFullName,
                                   *offset,
                                   static_cast<int32>(*offset + term.size())});
            }
            const std::string joined_addresses = JoinLines(entry.addresses);
            if (const auto offset = FindCaseInsensitive(joined_addresses, term)) {
                matches.push_back({index,
                                   MatchField::kAddresses,
                                   *offset,
                                   static_cast<int32>(*offset + term.size())});
            }
            if (const auto offset = FindCaseInsensitive(entry.notes, term)) {
                matches.push_back({index,
                                   MatchField::kNotes,
                                   *offset,
                                   static_cast<int32>(*offset + term.size())});
            }
            const std::string recipient_text =
                std::string("include in recipient list ") + (entry.recipient_list ? "on" : "off");
            if (const auto offset = FindCaseInsensitive(recipient_text, term)) {
                matches.push_back({index,
                                   MatchField::kRecipientList,
                                   *offset,
                                   static_cast<int32>(*offset + term.size())});
            }
            const std::string bp_text =
                std::string("include in boss protector list ") + (entry.bp_list ? "on" : "off");
            if (const auto offset = FindCaseInsensitive(bp_text, term)) {
                matches.push_back({index,
                                   MatchField::kBossProtector,
                                   *offset,
                                   static_cast<int32>(*offset + term.size())});
            }
        }
        if (matches.empty()) {
            return std::nullopt;
        }
        if (!repeat || !last_find_result_ || !EqualsCaseInsensitive(term, last_find_term_)) {
            return matches.front();
        }

        const auto after_current = std::find_if(matches.begin(), matches.end(), [&](const auto& match) {
            if (match.entry_index != last_find_result_->entry_index) {
                return match.entry_index > last_find_result_->entry_index;
            }
            return static_cast<int>(match.field) > static_cast<int>(last_find_result_->field);
        });
        return after_current != matches.end() ? std::optional<MatchResult>(*after_current)
                                              : std::optional<MatchResult>(matches.front());
    }

    void ApplyMatch(const MatchResult& match) {
        SetDetailsVisible(true);
        SelectEntryAt(static_cast<int32>(match.entry_index));

        switch (match.field) {
            case MatchField::kNickname:
                tabs_->Select(0);
                if (auto* view = nickname_control_->TextView()) {
                    view->MakeFocus(true);
                    view->Select(match.start, match.end);
                    preferred_focus_view_ = view;
                }
                break;
            case MatchField::kFullName:
                tabs_->Select(0);
                if (auto* view = full_name_control_->TextView()) {
                    view->MakeFocus(true);
                    view->Select(match.start, match.end);
                    preferred_focus_view_ = view;
                }
                break;
            case MatchField::kAddresses:
                tabs_->Select(0);
                addresses_view_->MakeFocus(true);
                addresses_view_->Select(match.start, match.end);
                preferred_focus_view_ = addresses_view_;
                break;
            case MatchField::kNotes:
                tabs_->Select(1);
                notes_view_->MakeFocus(true);
                notes_view_->Select(match.start, match.end);
                preferred_focus_view_ = notes_view_;
                break;
            case MatchField::kRecipientList:
                tabs_->Select(2);
                recipient_box_->MakeFocus(true);
                preferred_focus_view_ = recipient_box_;
                break;
            case MatchField::kBossProtector:
                tabs_->Select(2);
                bp_box_->MakeFocus(true);
                preferred_focus_view_ = bp_box_;
                break;
        }
    }

    void ShowContextMenu(BPoint where) {
        BPopUpMenu menu("nicknames-context", false, false);
        auto* new_item = new BMenuItem("New", new BMessage(kNicknameNewMessage));
        auto* duplicate_item = new BMenuItem("Duplicate", new BMessage(kNicknameDuplicateMessage));
        auto* save_item = new BMenuItem("Save", new BMessage(kNicknameSaveMessage));
        auto* delete_item = new BMenuItem("Delete", new BMessage(kNicknameDeleteMessage));
        auto* compose_item = new BMenuItem("New Message To", new BMessage(kNicknameComposeMessage));
        auto* toggle_item = new BMenuItem(details_visible_ ? "Hide Details" : "Show Details",
                                          new BMessage(kWazooToggleDetailsMessage));
        duplicate_item->SetEnabled(IsCommandEnabled(kNicknameDuplicateMessage));
        save_item->SetEnabled(IsCommandEnabled(kNicknameSaveMessage));
        delete_item->SetEnabled(IsCommandEnabled(kNicknameDeleteMessage));
        compose_item->SetEnabled(IsCommandEnabled(kNicknameComposeMessage));
        menu.AddItem(new_item);
        menu.AddItem(duplicate_item);
        menu.AddItem(save_item);
        menu.AddItem(delete_item);
        menu.AddSeparatorItem();
        menu.AddItem(toggle_item);
        menu.AddSeparatorItem();
        menu.AddItem(compose_item);
        menu.SetTargetForItems(this);
        list_view_->ConvertToScreen(&where);
        menu.Go(where, true, false, true);
    }

    void ComposeToSelected() {
        const auto selected = SelectedEntry();
        if (!selected) {
            return;
        }
        ComposeMessage message;
        message.headers.to = JoinAddresses(selected->addresses);
        shell_host_.OpenComposer(message);
    }

    ContextListView* list_view_ = nullptr;
    BSplitView* split_view_ = nullptr;
    BGroupView* editor_group_ = nullptr;
    BTabView* tabs_ = nullptr;
    BTextControl* nickname_control_ = nullptr;
    BTextControl* full_name_control_ = nullptr;
    BTextView* addresses_view_ = nullptr;
    BTextView* notes_view_ = nullptr;
    BCheckBox* recipient_box_ = nullptr;
    BCheckBox* bp_box_ = nullptr;
    std::vector<NicknameEntry> entries_;
    std::string current_nickname_;
    NicknameEntry loaded_entry_;
    std::string last_find_term_;
    std::optional<MatchResult> last_find_result_;
    std::string remembered_split_layout_;
    BView* preferred_focus_view_ = nullptr;
    bool details_visible_ = true;
    bool suppress_selection_message_ = false;
};

class SignatureEditorPane final : public WazooPaneBase {
public:
    explicit SignatureEditorPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "signatures"),
          surface_(std::make_unique<hermes::PaigeRichTextSurface>(shell_host.Runtime())) {
        list_view_ = new ContextListView("signatures-list",
                                         new BMessage(kSignatureSelectionMessage),
                                         nullptr,
                                         kPaneDeleteMessage,
                                         [this](BPoint where) { ShowContextMenu(where); });
        name_control_ = new BTextControl("signature-name", "Name", "", nullptr);
        editor_view_ = new PaigeEditorView(*surface_);
        editor_view_->SetChangeCallback([this]() { dirty_ = true; });

        auto* editor_group = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(editor_group, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(name_control_)
            .Add(new BScrollView("signature-editor-scroll", editor_view_, 0, true, true));

        auto* split_view = new BSplitView(B_HORIZONTAL);
        split_view->AddChild(new BScrollView("signatures-list-scroll", list_view_, 0, false, true), 0.28f);
        split_view->AddChild(editor_group, 0.72f);

        auto* new_button = new BButton("signature-new", "New", new BMessage(kSignatureNewMessage));
        auto* duplicate_button =
            new BButton("signature-duplicate", "Duplicate", new BMessage(kSignatureDuplicateMessage));
        auto* save_button = new BButton("signature-save", "Save", new BMessage(kSignatureSaveMessage));
        auto* delete_button = new BButton("signature-delete", "Delete", new BMessage(kSignatureDeleteMessage));
        auto* reveal_button =
            new BButton("signature-reveal", "Reveal on Disk", new BMessage(kSignatureRevealMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(split_view)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new_button)
                .Add(duplicate_button)
                .Add(save_button)
                .Add(delete_button)
                .AddGlue()
                .Add(reveal_button)
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
    }

    bool HandleCommand(uint32 command_id) override {
        switch (command_id) {
            case kSignatureNewMessage:
            case kPaneNewMessage:
                ClearEditor();
                return true;
            case kSignatureDuplicateMessage:
                DuplicateSelected();
                return true;
            case kSignatureSaveMessage:
            case kPaneSaveMessage:
                SaveCurrent();
                return true;
            case kSignatureDeleteMessage:
            case kPaneDeleteMessage:
                DeleteSelected();
                return true;
            case kSignatureRevealMessage:
                RevealSelected();
                return true;
            default:
                return false;
        }
    }

    bool IsCommandEnabled(uint32 command_id) const override {
        const bool has_selection = SelectedTemplate().has_value();
        switch (command_id) {
            case kSignatureNewMessage:
            case kPaneNewMessage:
                return true;
            case kSignatureDuplicateMessage:
                return has_selection;
            case kSignatureSaveMessage:
            case kPaneSaveMessage:
                return !TrimWhitespace(name_control_->Text() != nullptr ? name_control_->Text() : "").empty();
            case kSignatureDeleteMessage:
            case kPaneDeleteMessage:
                return has_selection;
            case kSignatureRevealMessage:
                return has_selection;
            default:
                return false;
        }
    }

    BView* PreferredFocusView() const override {
        return list_view_;
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kSignatureSelectionMessage:
                LoadSelected();
                return;
            case kSignatureNewMessage:
                ClearEditor();
                return;
            case kSignatureDuplicateMessage:
                DuplicateSelected();
                return;
            case kSignatureSaveMessage:
                SaveCurrent();
                return;
            case kSignatureDeleteMessage:
                DeleteSelected();
                return;
            case kSignatureRevealMessage:
                RevealSelected();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        templates_ = shell_host_.Signatures().Templates();
        list_view_->MakeEmpty();
        int32 selected_index = -1;
        for (std::size_t index = 0; index < templates_.size(); ++index) {
            list_view_->AddItem(new BStringItem(templates_[index].name.c_str()));
            if (!current_name_.empty() && templates_[index].name == current_name_) {
                selected_index = static_cast<int32>(index);
            }
        }
        if (selected_index < 0 && !templates_.empty()) {
            selected_index = 0;
        }
        if (selected_index >= 0) {
            list_view_->Select(selected_index);
            LoadSelected();
        } else {
            ClearEditor();
        }
    }

private:
    std::optional<SignatureTemplate> SelectedTemplate() const {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= templates_.size()) {
            return std::nullopt;
        }
        return templates_[static_cast<std::size_t>(index)];
    }

    void ClearEditor() {
        current_name_.clear();
        name_control_->SetText("");
        surface_->Load(RichTextDocument{});
        editor_view_->ReloadFromSurface();
        list_view_->DeselectAll();
        dirty_ = false;
    }

    void LoadSelected() {
        const auto selected = SelectedTemplate();
        if (!selected) {
            ClearEditor();
            return;
        }
        current_name_ = selected->name;
        name_control_->SetText(selected->name.c_str());
        surface_->Load(selected->body);
        editor_view_->ReloadFromSurface();
        dirty_ = false;
    }

    void SaveCurrent() {
        SignatureTemplate signature;
        signature.name = name_control_->Text() != nullptr ? name_control_->Text() : "";
        signature.body = surface_->Snapshot();
        if (signature.name.empty()) {
            return;
        }
        std::string error_message;
        if (!shell_host_.Signatures().SaveTemplate(signature, &error_message)) {
            return;
        }
        if (!current_name_.empty() && current_name_ != signature.name) {
            shell_host_.Signatures().DeleteTemplate(current_name_, &error_message);
        }
        shell_host_.Signatures().Discover(shell_host_.Signatures().RootDirectory(), &error_message);
        current_name_ = signature.name;
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void DeleteSelected() {
        const auto selected = SelectedTemplate();
        if (!selected) {
            return;
        }
        std::string error_message;
        shell_host_.Signatures().DeleteTemplate(selected->name, &error_message);
        shell_host_.Signatures().Discover(shell_host_.Signatures().RootDirectory(), &error_message);
        current_name_.clear();
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void DuplicateSelected() {
        const auto selected = SelectedTemplate();
        if (!selected) {
            return;
        }
        SignatureTemplate duplicate = *selected;
        duplicate.name += " Copy";
        std::string error_message;
        shell_host_.Signatures().SaveTemplate(duplicate, &error_message);
        shell_host_.Signatures().Discover(shell_host_.Signatures().RootDirectory(), &error_message);
        current_name_ = duplicate.name;
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void RevealSelected() {
        const auto selected = SelectedTemplate();
        if (!selected) {
            return;
        }
        const auto path = selected->source_path.parent_path().empty() ? selected->source_path
                                                                      : selected->source_path.parent_path();
        (void)LaunchPath(path);
    }

    void ShowContextMenu(BPoint where) {
        BPopUpMenu menu("signatures-context", false, false);
        auto* duplicate_item =
            new BMenuItem("Duplicate", new BMessage(kSignatureDuplicateMessage));
        auto* save_item = new BMenuItem("Save", new BMessage(kSignatureSaveMessage));
        auto* delete_item = new BMenuItem("Delete", new BMessage(kSignatureDeleteMessage));
        auto* reveal_item =
            new BMenuItem("Reveal on Disk", new BMessage(kSignatureRevealMessage));
        duplicate_item->SetEnabled(IsCommandEnabled(kSignatureDuplicateMessage));
        save_item->SetEnabled(IsCommandEnabled(kSignatureSaveMessage));
        delete_item->SetEnabled(IsCommandEnabled(kSignatureDeleteMessage));
        reveal_item->SetEnabled(IsCommandEnabled(kSignatureRevealMessage));
        menu.AddItem(new BMenuItem("New", new BMessage(kSignatureNewMessage)));
        menu.AddItem(duplicate_item);
        menu.AddItem(save_item);
        menu.AddItem(delete_item);
        menu.AddSeparatorItem();
        menu.AddItem(reveal_item);
        menu.SetTargetForItems(this);
        list_view_->ConvertToScreen(&where);
        menu.Go(where, true, false, true);
    }

    ContextListView* list_view_ = nullptr;
    BTextControl* name_control_ = nullptr;
    PaigeEditorView* editor_view_ = nullptr;
    std::unique_ptr<hermes::PaigeRichTextSurface> surface_;
    std::vector<SignatureTemplate> templates_;
    std::string current_name_;
    bool dirty_ = false;
};

class StationeryEditorPane final : public WazooPaneBase {
public:
    explicit StationeryEditorPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "stationery"),
          surface_(std::make_unique<hermes::PaigeRichTextSurface>(shell_host.Runtime())) {
        list_view_ = new ContextListView("stationery-list",
                                         new BMessage(kStationerySelectionMessage),
                                         nullptr,
                                         kPaneDeleteMessage,
                                         [this](BPoint where) { ShowContextMenu(where); });
        name_control_ = new BTextControl("stationery-name", "Name", "", nullptr);
        to_control_ = new BTextControl("stationery-to", "To", "", nullptr);
        cc_control_ = new BTextControl("stationery-cc", "Cc", "", nullptr);
        bcc_control_ = new BTextControl("stationery-bcc", "Bcc", "", nullptr);
        subject_control_ = new BTextControl("stationery-subject", "Subject", "", nullptr);
        persona_field_ = BuildMenuField("stationery-persona", "Persona", {""});
        signature_field_ = BuildMenuField("stationery-signature", "Signature", {""});
        editor_view_ = new PaigeEditorView(*surface_);
        editor_view_->SetChangeCallback([this]() { dirty_ = true; });

        auto* editor_group = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(editor_group, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(name_control_)
            .Add(to_control_)
            .Add(cc_control_)
            .Add(bcc_control_)
            .Add(subject_control_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(persona_field_)
                .Add(signature_field_)
            .End()
            .Add(new BScrollView("stationery-editor-scroll", editor_view_, 0, true, true));

        auto* split_view = new BSplitView(B_HORIZONTAL);
        split_view->AddChild(new BScrollView("stationery-list-scroll", list_view_, 0, false, true), 0.28f);
        split_view->AddChild(editor_group, 0.72f);

        auto* new_button = new BButton("stationery-new", "New", new BMessage(kStationeryNewMessage));
        auto* duplicate_button =
            new BButton("stationery-duplicate", "Duplicate", new BMessage(kStationeryDuplicateMessage));
        auto* save_button = new BButton("stationery-save", "Save", new BMessage(kStationerySaveMessage));
        auto* delete_button = new BButton("stationery-delete", "Delete", new BMessage(kStationeryDeleteMessage));
        auto* use_button =
            new BButton("stationery-use", "Use to Compose", new BMessage(kStationeryUseMessage));
        auto* reply_button =
            new BButton("stationery-reply", "Reply With", new BMessage(kStationeryReplyMessage));
        auto* reply_all_button =
            new BButton("stationery-reply-all", "Reply All With", new BMessage(kStationeryReplyAllMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(split_view)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new_button)
                .Add(duplicate_button)
                .Add(save_button)
                .Add(delete_button)
                .AddGlue()
                .Add(use_button)
                .Add(reply_button)
                .Add(reply_all_button)
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
    }

    bool HandleCommand(uint32 command_id) override {
        switch (command_id) {
            case kStationeryNewMessage:
            case kPaneNewMessage:
                ClearEditor();
                return true;
            case kStationeryDuplicateMessage:
                DuplicateSelected();
                return true;
            case kStationerySaveMessage:
            case kPaneSaveMessage:
                SaveCurrent();
                return true;
            case kStationeryDeleteMessage:
            case kPaneDeleteMessage:
                DeleteSelected();
                return true;
            case kStationeryUseMessage:
                UseSelected();
                return true;
            case kStationeryReplyMessage:
                ReplyWithSelected(false);
                return true;
            case kStationeryReplyAllMessage:
                ReplyWithSelected(true);
                return true;
            default:
                return false;
        }
    }

    bool IsCommandEnabled(uint32 command_id) const override {
        const bool has_selection = SelectedTemplate().has_value();
        const bool can_reply = has_selection && !shell_host_.ActiveMailboxId().empty() &&
                               !shell_host_.ActiveMessageId().empty();
        switch (command_id) {
            case kStationeryNewMessage:
            case kPaneNewMessage:
                return true;
            case kStationeryDuplicateMessage:
                return has_selection;
            case kStationerySaveMessage:
            case kPaneSaveMessage:
                return !TrimWhitespace(name_control_->Text() != nullptr ? name_control_->Text() : "").empty();
            case kStationeryDeleteMessage:
            case kPaneDeleteMessage:
                return has_selection;
            case kStationeryUseMessage:
                return has_selection;
            case kStationeryReplyMessage:
            case kStationeryReplyAllMessage:
                return can_reply;
            default:
                return false;
        }
    }

    BView* PreferredFocusView() const override {
        return list_view_;
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kStationerySelectionMessage:
                LoadSelected();
                return;
            case kStationeryNewMessage:
                ClearEditor();
                return;
            case kStationeryDuplicateMessage:
                DuplicateSelected();
                return;
            case kStationerySaveMessage:
                SaveCurrent();
                return;
            case kStationeryDeleteMessage:
                DeleteSelected();
                return;
            case kStationeryUseMessage:
                UseSelected();
                return;
            case kStationeryReplyMessage:
                ReplyWithSelected(false);
                return;
            case kStationeryReplyAllMessage:
                ReplyWithSelected(true);
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        templates_ = shell_host_.Stationery().Templates();
        list_view_->MakeEmpty();
        RepopulateMenus();
        int32 selected_index = -1;
        for (std::size_t index = 0; index < templates_.size(); ++index) {
            list_view_->AddItem(new BStringItem(templates_[index].name.c_str()));
            if (!current_name_.empty() && templates_[index].name == current_name_) {
                selected_index = static_cast<int32>(index);
            }
        }
        if (selected_index < 0 && !templates_.empty()) {
            selected_index = 0;
        }
        if (selected_index >= 0) {
            list_view_->Select(selected_index);
            LoadSelected();
        } else {
            ClearEditor();
        }
    }

private:
    std::optional<StationeryTemplate> SelectedTemplate() const {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= templates_.size()) {
            return std::nullopt;
        }
        return templates_[static_cast<std::size_t>(index)];
    }

    void RepopulateMenus() {
        if (persona_field_ != nullptr && persona_field_->Menu() != nullptr) {
            persona_field_->Menu()->RemoveItems(0, persona_field_->Menu()->CountItems(), true);
            persona_field_->Menu()->AddItem(new BMenuItem("", nullptr));
            for (const auto& account : shell_host_.Accounts().Accounts()) {
                const std::string label =
                    account.display_name.empty() ? account.id : account.display_name;
                persona_field_->Menu()->AddItem(new BMenuItem(label.c_str(), nullptr));
            }
        }
        if (signature_field_ != nullptr && signature_field_->Menu() != nullptr) {
            signature_field_->Menu()->RemoveItems(0, signature_field_->Menu()->CountItems(), true);
            signature_field_->Menu()->AddItem(new BMenuItem("", nullptr));
            for (const auto& signature : shell_host_.Signatures().Templates()) {
                signature_field_->Menu()->AddItem(new BMenuItem(signature.name.c_str(), nullptr));
            }
        }
    }

    void ClearEditor() {
        current_name_.clear();
        name_control_->SetText("");
        to_control_->SetText("");
        cc_control_->SetText("");
        bcc_control_->SetText("");
        subject_control_->SetText("");
        SetMarkedMenuValue(persona_field_, "");
        SetMarkedMenuValue(signature_field_, "");
        surface_->Load(RichTextDocument{});
        editor_view_->ReloadFromSurface();
        list_view_->DeselectAll();
        dirty_ = false;
    }

    void LoadSelected() {
        const auto selected = SelectedTemplate();
        if (!selected) {
            ClearEditor();
            return;
        }
        current_name_ = selected->name;
        name_control_->SetText(selected->name.c_str());
        to_control_->SetText(selected->headers.to.c_str());
        cc_control_->SetText(selected->headers.cc.c_str());
        bcc_control_->SetText(selected->headers.bcc.c_str());
        subject_control_->SetText(selected->headers.subject.c_str());
        SetMarkedMenuValue(persona_field_, selected->persona);
        SetMarkedMenuValue(signature_field_, selected->signature_name);
        surface_->Load(selected->body);
        editor_view_->ReloadFromSurface();
        dirty_ = false;
    }

    void SaveCurrent() {
        StationeryTemplate entry;
        entry.name = name_control_->Text() != nullptr ? name_control_->Text() : "";
        entry.headers.to = to_control_->Text() != nullptr ? to_control_->Text() : "";
        entry.headers.cc = cc_control_->Text() != nullptr ? cc_control_->Text() : "";
        entry.headers.bcc = bcc_control_->Text() != nullptr ? bcc_control_->Text() : "";
        entry.headers.subject = subject_control_->Text() != nullptr ? subject_control_->Text() : "";
        entry.persona = MenuValue(persona_field_);
        entry.signature_name = MenuValue(signature_field_);
        entry.body = surface_->Snapshot();
        if (entry.name.empty()) {
            return;
        }

        std::string error_message;
        if (!shell_host_.Stationery().SaveTemplate(entry, &error_message)) {
            return;
        }
        if (!current_name_.empty() && current_name_ != entry.name) {
            shell_host_.Stationery().DeleteTemplate(current_name_, &error_message);
        }
        shell_host_.Stationery().Discover(shell_host_.Stationery().RootDirectory(), &error_message);
        current_name_ = entry.name;
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void DeleteSelected() {
        const auto selected = SelectedTemplate();
        if (!selected) {
            return;
        }
        std::string error_message;
        shell_host_.Stationery().DeleteTemplate(selected->name, &error_message);
        shell_host_.Stationery().Discover(shell_host_.Stationery().RootDirectory(), &error_message);
        current_name_.clear();
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void DuplicateSelected() {
        const auto selected = SelectedTemplate();
        if (!selected) {
            return;
        }
        StationeryTemplate duplicate = *selected;
        duplicate.name += " Copy";
        std::string error_message;
        shell_host_.Stationery().SaveTemplate(duplicate, &error_message);
        shell_host_.Stationery().Discover(shell_host_.Stationery().RootDirectory(), &error_message);
        current_name_ = duplicate.name;
        shell_host_.ReloadWorkspace();
        Refresh();
    }

    void UseSelected() {
        const auto selected = SelectedTemplate();
        if (!selected) {
            return;
        }
        ComposeMessage message;
        message.headers.to = selected->headers.to;
        message.headers.cc = selected->headers.cc;
        message.headers.bcc = selected->headers.bcc;
        message.headers.subject = selected->headers.subject;
        message.headers.from_persona = selected->persona;
        message.body = selected->body;
        message.stationery_name = selected->name;
        message.signature_name = selected->signature_name;
        shell_host_.OpenComposer(message);
    }

    void ReplyWithSelected(bool reply_all) {
        const auto selected = SelectedTemplate();
        if (!selected || shell_host_.ActiveMailboxId().empty() || shell_host_.ActiveMessageId().empty()) {
            return;
        }
        const auto response = shell_host_.BuildResponseMessage(
            reply_all ? HaikuShellHost::MessageResponseKind::kReplyAll
                      : HaikuShellHost::MessageResponseKind::kReply,
            shell_host_.ActiveMailboxId(),
            shell_host_.ActiveMessageId(),
            selected->name);
        if (response) {
            shell_host_.OpenComposer(*response);
        }
    }

    void ShowContextMenu(BPoint where) {
        BPopUpMenu menu("stationery-context", false, false);
        auto* duplicate_item =
            new BMenuItem("Duplicate", new BMessage(kStationeryDuplicateMessage));
        auto* save_item = new BMenuItem("Save", new BMessage(kStationerySaveMessage));
        auto* delete_item = new BMenuItem("Delete", new BMessage(kStationeryDeleteMessage));
        auto* compose_item =
            new BMenuItem("New Message With", new BMessage(kStationeryUseMessage));
        auto* reply_item =
            new BMenuItem("Reply With", new BMessage(kStationeryReplyMessage));
        auto* reply_all_item =
            new BMenuItem("Reply to All With", new BMessage(kStationeryReplyAllMessage));
        duplicate_item->SetEnabled(IsCommandEnabled(kStationeryDuplicateMessage));
        save_item->SetEnabled(IsCommandEnabled(kStationerySaveMessage));
        delete_item->SetEnabled(IsCommandEnabled(kStationeryDeleteMessage));
        compose_item->SetEnabled(IsCommandEnabled(kStationeryUseMessage));
        reply_item->SetEnabled(IsCommandEnabled(kStationeryReplyMessage));
        reply_all_item->SetEnabled(IsCommandEnabled(kStationeryReplyAllMessage));
        menu.AddItem(new BMenuItem("New", new BMessage(kStationeryNewMessage)));
        menu.AddItem(duplicate_item);
        menu.AddItem(save_item);
        menu.AddItem(delete_item);
        menu.AddSeparatorItem();
        menu.AddItem(compose_item);
        menu.AddItem(reply_item);
        menu.AddItem(reply_all_item);
        menu.SetTargetForItems(this);
        list_view_->ConvertToScreen(&where);
        menu.Go(where, true, false, true);
    }

    ContextListView* list_view_ = nullptr;
    BTextControl* name_control_ = nullptr;
    BTextControl* to_control_ = nullptr;
    BTextControl* cc_control_ = nullptr;
    BTextControl* bcc_control_ = nullptr;
    BTextControl* subject_control_ = nullptr;
    BMenuField* persona_field_ = nullptr;
    BMenuField* signature_field_ = nullptr;
    PaigeEditorView* editor_view_ = nullptr;
    std::unique_ptr<hermes::PaigeRichTextSurface> surface_;
    std::vector<StationeryTemplate> templates_;
    std::string current_name_;
    bool dirty_ = false;
};

class FilterManagerPane final : public WazooPaneBase {
public:
    explicit FilterManagerPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "filters") {
        list_view_ = new ContextListView("filters-list",
                                         new BMessage(kFilterSelectionMessage),
                                         nullptr,
                                         kFilterDeleteMessage,
                                         [this](BPoint where) { ShowContextMenu(where); });
        split_view_ = new BSplitView(B_HORIZONTAL);
        name_control_ = new BTextControl("filter-name", "Name", "", nullptr);
        field_menu_ = BuildMenuField("filter-field", "Field", {"From", "To", "Subject", "Body"});
        operation_menu_ =
            BuildMenuField("filter-operation", "Operation", {"Contains", "Equals", "Does Not Contain"});
        value_control_ = new BTextControl("filter-value", "Value", "", nullptr);
        destination_control_ = new BTextControl("filter-destination", "Destination", "", nullptr);
        mark_as_read_box_ = new BCheckBox("filter-mark-read", "Mark as read", nullptr);
        mark_as_junk_box_ = new BCheckBox("filter-mark-junk", "Mark as junk", nullptr);
        stop_processing_box_ =
            new BCheckBox("filter-stop-processing", "Stop processing after match", nullptr);

        auto* editor_group = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(editor_group, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(name_control_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(field_menu_)
                .Add(operation_menu_)
            .End()
            .Add(value_control_)
            .Add(destination_control_)
            .Add(mark_as_read_box_)
            .Add(mark_as_junk_box_)
            .Add(stop_processing_box_)
            .AddGlue();

        split_view_->AddChild(new BScrollView("filters-list-scroll", list_view_, 0, false, true), 0.32f);
        split_view_->AddChild(editor_group, 0.68f);

        auto* new_button = new BButton("filter-new", "New", new BMessage(kFilterNewMessage));
        auto* save_button = new BButton("filter-save", "Save", new BMessage(kFilterSaveMessage));
        auto* delete_button = new BButton("filter-delete", "Delete", new BMessage(kFilterDeleteMessage));
        auto* move_up_button = new BButton("filter-up", "Move Up", new BMessage(kFilterMoveUpMessage));
        auto* move_down_button =
            new BButton("filter-down", "Move Down", new BMessage(kFilterMoveDownMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(split_view_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new_button)
                .Add(save_button)
                .Add(delete_button)
                .Add(move_up_button)
                .Add(move_down_button)
                .AddGlue()
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
        preferred_focus_view_ = list_view_;
    }

    void PersistState() override {
        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        preferences.filters_split_layout = SerializeSplitWeights(*split_view_);
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kFilterSelectionMessage:
                HandleSelectionChange();
                return;
            case kFilterNewMessage:
                HandleCommand(kFilterNewMessage);
                return;
            case kFilterSaveMessage:
                HandleCommand(kFilterSaveMessage);
                return;
            case kFilterDeleteMessage:
                HandleCommand(kFilterDeleteMessage);
                return;
            case kFilterMoveUpMessage:
                HandleCommand(kFilterMoveUpMessage);
                return;
            case kFilterMoveDownMessage:
                HandleCommand(kFilterMoveDownMessage);
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        rules_ = shell_host_.Filters().Rules();
        list_view_->MakeEmpty();
        int32 selected_index = -1;
        for (std::size_t index = 0; index < rules_.size(); ++index) {
            list_view_->AddItem(new BStringItem(rules_[index].name.c_str()));
            if (!current_rule_name_.empty() && rules_[index].name == current_rule_name_) {
                selected_index = static_cast<int32>(index);
            }
        }
        if (selected_index < 0 && !rules_.empty()) {
            selected_index = 0;
        }
        if (selected_index >= 0) {
            SelectRuleAt(selected_index);
        } else {
            ClearEditor();
        }
        RestoreSplitWeights(*split_view_, GuiPreferencesFromSettings(shell_host_.Settings()).filters_split_layout);
    }

    bool HandleCommand(uint32 command_id) override {
        switch (command_id) {
            case kPaneNewMessage:
            case kFilterNewMessage:
                if (!ResolveDirtyState("creating a new filter")) {
                    return false;
                }
                ClearEditor();
                preferred_focus_view_ = name_control_->TextView();
                if (preferred_focus_view_ != nullptr) {
                    preferred_focus_view_->MakeFocus(true);
                }
                return true;
            case kPaneSaveMessage:
            case kFilterSaveMessage:
                return SaveCurrent();
            case kPaneDeleteMessage:
            case kFilterDeleteMessage:
                if (!ResolveDirtyState("deleting this filter")) {
                    return false;
                }
                DeleteSelected();
                return true;
            case kFilterMoveUpMessage:
                if (!ResolveDirtyState("reordering filters")) {
                    return false;
                }
                MoveSelected(-1);
                return true;
            case kFilterMoveDownMessage:
                if (!ResolveDirtyState("reordering filters")) {
                    return false;
                }
                MoveSelected(1);
                return true;
            default:
                return false;
        }
    }

    bool IsCommandEnabled(uint32 command_id) const override {
        switch (command_id) {
            case kPaneNewMessage:
            case kFilterNewMessage:
                return true;
            case kPaneSaveMessage:
            case kFilterSaveMessage:
                return !BuildRule().name.empty() && IsDirty();
            case kPaneDeleteMessage:
            case kFilterDeleteMessage:
                return SelectedIndex().has_value();
            case kFilterMoveUpMessage:
                return SelectedIndex().has_value() && *SelectedIndex() > 0;
            case kFilterMoveDownMessage:
                return SelectedIndex().has_value() && *SelectedIndex() + 1 < rules_.size();
            default:
                return false;
        }
    }

    bool CanFind() const override {
        return !rules_.empty();
    }

    bool HandleFind(std::string_view term, bool repeat) override {
        const auto match = FindMatch(term, repeat);
        if (!match) {
            return false;
        }
        ApplyMatch(*match);
        last_find_term_ = std::string(term);
        last_find_result_ = *match;
        return true;
    }

    BView* PreferredFocusView() const override {
        return preferred_focus_view_ != nullptr ? preferred_focus_view_ : list_view_;
    }

    bool CanDeactivate() override {
        return ResolveDirtyState("switching away from Filters");
    }

private:
    enum class MatchField {
        kName = 0,
        kField,
        kOperation,
        kValue,
        kDestination,
    };

    struct MatchResult {
        std::size_t rule_index = 0;
        MatchField field = MatchField::kName;
        int32 start = 0;
        int32 end = 0;
    };

    std::optional<std::size_t> SelectedIndex() const {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= rules_.size()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(index);
    }

    bool IsDirty() const {
        if (current_rule_name_.empty()) {
            const FilterRule rule = BuildRule();
            return !rule.name.empty() || !rule.value.empty() || rule.destination_mailbox.has_value() ||
                   rule.mark_as_read || rule.mark_as_junk || !rule.stop_processing;
        }
        return BuildRule().name != loaded_rule_.name || BuildRule().field != loaded_rule_.field ||
               BuildRule().operation != loaded_rule_.operation || BuildRule().value != loaded_rule_.value ||
               BuildRule().destination_mailbox != loaded_rule_.destination_mailbox ||
               BuildRule().mark_as_read != loaded_rule_.mark_as_read ||
               BuildRule().mark_as_junk != loaded_rule_.mark_as_junk ||
               BuildRule().stop_processing != loaded_rule_.stop_processing;
    }

    void ClearEditor() {
        current_rule_name_.clear();
        loaded_rule_ = FilterRule{};
        name_control_->SetText("");
        value_control_->SetText("");
        destination_control_->SetText("");
        SetMarkedMenuValue(field_menu_, "Subject");
        SetMarkedMenuValue(operation_menu_, "Contains");
        mark_as_read_box_->SetValue(B_CONTROL_OFF);
        mark_as_junk_box_->SetValue(B_CONTROL_OFF);
        stop_processing_box_->SetValue(B_CONTROL_ON);
        suppress_selection_message_ = true;
        list_view_->DeselectAll();
        suppress_selection_message_ = false;
        preferred_focus_view_ = list_view_;
    }

    void LoadSelected() {
        const auto index = SelectedIndex();
        if (!index) {
            ClearEditor();
            return;
        }
        const auto& rule = rules_[*index];
        current_rule_name_ = rule.name;
        loaded_rule_ = rule;
        name_control_->SetText(rule.name.c_str());
        SetMarkedMenuValue(field_menu_, FilterFieldLabel(rule.field));
        SetMarkedMenuValue(operation_menu_, FilterOperationLabel(rule.operation));
        value_control_->SetText(rule.value.c_str());
        destination_control_->SetText(rule.destination_mailbox ? rule.destination_mailbox->c_str() : "");
        mark_as_read_box_->SetValue(rule.mark_as_read ? B_CONTROL_ON : B_CONTROL_OFF);
        mark_as_junk_box_->SetValue(rule.mark_as_junk ? B_CONTROL_ON : B_CONTROL_OFF);
        stop_processing_box_->SetValue(rule.stop_processing ? B_CONTROL_ON : B_CONTROL_OFF);
        preferred_focus_view_ = list_view_;
    }

    FilterRule BuildRule() const {
        FilterRule rule;
        rule.name = name_control_->Text() != nullptr ? name_control_->Text() : "";
        rule.field = FilterFieldFromLabel(MenuValue(field_menu_));
        rule.operation = FilterOperationFromLabel(MenuValue(operation_menu_));
        rule.value = value_control_->Text() != nullptr ? value_control_->Text() : "";
        const std::string destination =
            destination_control_->Text() != nullptr ? destination_control_->Text() : "";
        if (!destination.empty()) {
            rule.destination_mailbox = destination;
        }
        rule.mark_as_read = mark_as_read_box_->Value() == B_CONTROL_ON;
        rule.mark_as_junk = mark_as_junk_box_->Value() == B_CONTROL_ON;
        rule.stop_processing = stop_processing_box_->Value() == B_CONTROL_ON;
        return rule;
    }

    bool SaveRules() {
        shell_host_.Filters().SetRules(rules_);
        std::string error_message;
        const bool saved =
            shell_host_.Filters().SaveToFile(shell_host_.DataRootPath() / "Filters.ini", &error_message);
        shell_host_.ReloadWorkspace();
        return saved;
    }

    bool SaveCurrent() {
        FilterRule rule = BuildRule();
        if (rule.name.empty()) {
            return false;
        }

        bool replaced = false;
        for (auto& existing : rules_) {
            if (!current_rule_name_.empty() && existing.name == current_rule_name_) {
                existing = rule;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            shell_host_.Filters().AddOrReplace(rule);
            rules_ = shell_host_.Filters().Rules();
        } else {
            shell_host_.Filters().SetRules(rules_);
        }
        current_rule_name_ = rule.name;
        loaded_rule_ = rule;
        SaveRules();
        Refresh();
        return true;
    }

    void DeleteSelected() {
        const auto index = SelectedIndex();
        if (!index) {
            return;
        }
        rules_.erase(rules_.begin() + static_cast<std::ptrdiff_t>(*index));
        current_rule_name_.clear();
        SaveRules();
        Refresh();
    }

    void MoveSelected(int direction) {
        const auto index = SelectedIndex();
        if (!index) {
            return;
        }
        const int32 destination = static_cast<int32>(*index) + direction;
        if (destination < 0 || destination >= static_cast<int32>(rules_.size())) {
            return;
        }
        std::swap(rules_[*index], rules_[static_cast<std::size_t>(destination)]);
        current_rule_name_ = rules_[static_cast<std::size_t>(destination)].name;
        SaveRules();
        Refresh();
    }

    bool ResolveDirtyState(std::string_view action_label) {
        if (!IsDirty()) {
            return true;
        }
        const auto choice = PromptToSaveChanges(
            "Filters",
            std::string("Save changes before ").append(action_label).append("?"));
        if (choice == DirtyPromptAction::kSave) {
            return SaveCurrent();
        }
        if (choice == DirtyPromptAction::kDiscard) {
            if (current_rule_name_.empty()) {
                ClearEditor();
            } else {
                const auto it = std::find_if(rules_.begin(), rules_.end(), [&](const auto& rule) {
                    return rule.name == current_rule_name_;
                });
                if (it != rules_.end()) {
                    loaded_rule_ = *it;
                    name_control_->SetText(it->name.c_str());
                    SetMarkedMenuValue(field_menu_, FilterFieldLabel(it->field));
                    SetMarkedMenuValue(operation_menu_, FilterOperationLabel(it->operation));
                    value_control_->SetText(it->value.c_str());
                    destination_control_->SetText(
                        it->destination_mailbox ? it->destination_mailbox->c_str() : "");
                    mark_as_read_box_->SetValue(it->mark_as_read ? B_CONTROL_ON : B_CONTROL_OFF);
                    mark_as_junk_box_->SetValue(it->mark_as_junk ? B_CONTROL_ON : B_CONTROL_OFF);
                    stop_processing_box_->SetValue(it->stop_processing ? B_CONTROL_ON : B_CONTROL_OFF);
                } else {
                    ClearEditor();
                }
            }
            return true;
        }
        return false;
    }

    void SelectRuleAt(int32 index) {
        if (index < 0 || static_cast<std::size_t>(index) >= rules_.size()) {
            return;
        }
        suppress_selection_message_ = true;
        list_view_->Select(index);
        suppress_selection_message_ = false;
        LoadSelected();
    }

    void RestoreSelectionToCurrent() {
        int32 selected_index = -1;
        for (std::size_t index = 0; index < rules_.size(); ++index) {
            if (rules_[index].name == current_rule_name_) {
                selected_index = static_cast<int32>(index);
                break;
            }
        }
        suppress_selection_message_ = true;
        if (selected_index >= 0) {
            list_view_->Select(selected_index);
        } else {
            list_view_->DeselectAll();
        }
        suppress_selection_message_ = false;
    }

    void HandleSelectionChange() {
        if (suppress_selection_message_) {
            return;
        }
        const auto next_index = SelectedIndex();
        const std::string next_name = next_index ? rules_[*next_index].name : std::string();
        if (next_name == current_rule_name_) {
            LoadSelected();
            return;
        }
        if (!ResolveDirtyState("switching filters")) {
            RestoreSelectionToCurrent();
            return;
        }
        LoadSelected();
    }

    std::optional<MatchResult> FindMatch(std::string_view term, bool repeat) const {
        if (term.empty() || rules_.empty()) {
            return std::nullopt;
        }
        std::vector<MatchResult> matches;
        for (std::size_t index = 0; index < rules_.size(); ++index) {
            const auto& rule = rules_[index];
            if (const auto offset = FindCaseInsensitive(rule.name, term)) {
                matches.push_back({index, MatchField::kName, *offset, static_cast<int32>(*offset + term.size())});
            }
            const std::string field_label = FilterFieldLabel(rule.field);
            if (const auto offset = FindCaseInsensitive(field_label, term)) {
                matches.push_back({index, MatchField::kField, *offset, static_cast<int32>(*offset + term.size())});
            }
            const std::string operation_label = FilterOperationLabel(rule.operation);
            if (const auto offset = FindCaseInsensitive(operation_label, term)) {
                matches.push_back({index,
                                   MatchField::kOperation,
                                   *offset,
                                   static_cast<int32>(*offset + term.size())});
            }
            if (const auto offset = FindCaseInsensitive(rule.value, term)) {
                matches.push_back({index, MatchField::kValue, *offset, static_cast<int32>(*offset + term.size())});
            }
            const std::string destination = rule.destination_mailbox.value_or("");
            if (const auto offset = FindCaseInsensitive(destination, term)) {
                matches.push_back(
                    {index, MatchField::kDestination, *offset, static_cast<int32>(*offset + term.size())});
            }
        }
        if (matches.empty()) {
            return std::nullopt;
        }
        if (!repeat || !last_find_result_ || !EqualsCaseInsensitive(term, last_find_term_)) {
            return matches.front();
        }

        const auto after_current = std::find_if(matches.begin(), matches.end(), [&](const auto& match) {
            if (match.rule_index != last_find_result_->rule_index) {
                return match.rule_index > last_find_result_->rule_index;
            }
            return static_cast<int>(match.field) > static_cast<int>(last_find_result_->field);
        });
        return after_current != matches.end() ? std::optional<MatchResult>(*after_current)
                                              : std::optional<MatchResult>(matches.front());
    }

    void ApplyMatch(const MatchResult& match) {
        SelectRuleAt(static_cast<int32>(match.rule_index));
        switch (match.field) {
            case MatchField::kName:
                if (auto* view = name_control_->TextView()) {
                    view->MakeFocus(true);
                    view->Select(match.start, match.end);
                    preferred_focus_view_ = view;
                }
                break;
            case MatchField::kField:
                if (field_menu_ != nullptr) {
                    field_menu_->MakeFocus(true);
                    preferred_focus_view_ = field_menu_;
                }
                break;
            case MatchField::kOperation:
                if (operation_menu_ != nullptr) {
                    operation_menu_->MakeFocus(true);
                    preferred_focus_view_ = operation_menu_;
                }
                break;
            case MatchField::kValue:
                if (auto* view = value_control_->TextView()) {
                    view->MakeFocus(true);
                    view->Select(match.start, match.end);
                    preferred_focus_view_ = view;
                }
                break;
            case MatchField::kDestination:
                if (auto* view = destination_control_->TextView()) {
                    view->MakeFocus(true);
                    view->Select(match.start, match.end);
                    preferred_focus_view_ = view;
                }
                break;
        }
    }

    void ShowContextMenu(BPoint where) {
        BPopUpMenu menu("filters-context", false, false);
        auto* new_item = new BMenuItem("New", new BMessage(kFilterNewMessage));
        auto* save_item = new BMenuItem("Save", new BMessage(kFilterSaveMessage));
        auto* delete_item = new BMenuItem("Delete", new BMessage(kFilterDeleteMessage));
        auto* up_item = new BMenuItem("Move Up", new BMessage(kFilterMoveUpMessage));
        auto* down_item = new BMenuItem("Move Down", new BMessage(kFilterMoveDownMessage));
        save_item->SetEnabled(IsCommandEnabled(kFilterSaveMessage));
        delete_item->SetEnabled(IsCommandEnabled(kFilterDeleteMessage));
        up_item->SetEnabled(IsCommandEnabled(kFilterMoveUpMessage));
        down_item->SetEnabled(IsCommandEnabled(kFilterMoveDownMessage));
        menu.AddItem(new_item);
        menu.AddItem(save_item);
        menu.AddItem(delete_item);
        menu.AddSeparatorItem();
        menu.AddItem(up_item);
        menu.AddItem(down_item);
        menu.SetTargetForItems(this);
        list_view_->ConvertToScreen(&where);
        menu.Go(where, true, false, true);
    }

    ContextListView* list_view_ = nullptr;
    BSplitView* split_view_ = nullptr;
    BTextControl* name_control_ = nullptr;
    BMenuField* field_menu_ = nullptr;
    BMenuField* operation_menu_ = nullptr;
    BTextControl* value_control_ = nullptr;
    BTextControl* destination_control_ = nullptr;
    BCheckBox* mark_as_read_box_ = nullptr;
    BCheckBox* mark_as_junk_box_ = nullptr;
    BCheckBox* stop_processing_box_ = nullptr;
    std::vector<FilterRule> rules_;
    std::string current_rule_name_;
    FilterRule loaded_rule_;
    std::string last_find_term_;
    std::optional<MatchResult> last_find_result_;
    BView* preferred_focus_view_ = nullptr;
    bool suppress_selection_message_ = false;
};

class FilterReportTablePane final : public WazooPaneBase {
public:
    explicit FilterReportTablePane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "filter-report") {
        list_view_ = new BColumnListView("filter-report-list",
                                         B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS |
                                             B_FULL_UPDATE_ON_RESIZE);
        list_view_->SetSelectionMode(B_SINGLE_SELECTION_LIST);
        list_view_->SetSelectionMessage(new BMessage(kFilterReportSelectionMessage));
        list_view_->SetInvocationMessage(new BMessage(kFilterReportOpenMessage));
        list_view_->AddColumn(new BStringColumn("Timestamp", 148.0f, 112.0f, 220.0f, B_TRUNCATE_END),
                              kFilterReportFieldTimestamp);
        list_view_->AddColumn(new BStringColumn("Mailbox", 180.0f, 96.0f, 320.0f, B_TRUNCATE_END),
                              kFilterReportFieldMailbox);
        list_view_->AddColumn(new BStringColumn("Sender", 180.0f, 96.0f, 320.0f, B_TRUNCATE_END),
                              kFilterReportFieldSender);
        list_view_->AddColumn(new BStringColumn("Subject", 260.0f, 120.0f, 420.0f, B_TRUNCATE_END),
                              kFilterReportFieldSubject);
        list_view_->AddColumn(new BStringColumn("Matched Rules", 220.0f, 120.0f, 420.0f, B_TRUNCATE_END),
                              kFilterReportFieldRules);
        RestoreColumnListState(*list_view_,
                               GuiPreferencesFromSettings(shell_host_.Settings()).filter_report_column_layout);

        detail_view_ = new BTextView("filter-report-detail");
        detail_view_->MakeEditable(false);
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);

        auto* open_button = new BButton("filter-report-open", "Open Message", new BMessage(kFilterReportOpenMessage));
        auto* clear_button = new BButton("filter-report-clear", "Clear Report", new BMessage(kFilterReportClearMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new BScrollView("filter-report-scroll", list_view_, 0, false, true), 0.64f)
                .Add(new BScrollView("filter-report-detail-scroll", detail_view_, 0, false, true), 0.36f)
            .End()
            .AddGroup(B_HORIZONTAL, 8)
                .Add(open_button)
                .Add(clear_button)
                .AddGlue()
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
    }

    void PersistState() override {
        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        preferences.filter_report_column_layout = SerializeColumnListState(*list_view_);
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kFilterReportSelectionMessage:
                UpdateDetail();
                return;
            case kFilterReportOpenMessage:
                OpenSelected();
                return;
            case kFilterReportClearMessage:
                ClearReport();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        entries_ = shell_host_.FilterReport().Entries();
        std::stable_sort(entries_.begin(), entries_.end(), [](const auto& left, const auto& right) {
            return left.timestamp > right.timestamp;
        });
        list_view_->Clear();
        int32 selected_index = -1;
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            auto* row = new BRow();
            row->SetField(new BStringField(FormatTimestamp(entries_[index].timestamp).c_str()),
                          kFilterReportFieldTimestamp);
            row->SetField(new BStringField(entries_[index].mailbox_name.c_str()), kFilterReportFieldMailbox);
            row->SetField(new BStringField(entries_[index].sender.c_str()), kFilterReportFieldSender);
            row->SetField(new BStringField(entries_[index].subject.c_str()), kFilterReportFieldSubject);
            row->SetField(new BStringField(JoinLines(entries_[index].matched_rules).c_str()),
                          kFilterReportFieldRules);
            list_view_->AddRow(row);
            if (!current_entry_id_.empty() && entries_[index].id == current_entry_id_) {
                selected_index = static_cast<int32>(index);
            }
        }
        if (selected_index < 0 && !entries_.empty()) {
            selected_index = 0;
        }
        if (selected_index >= 0) {
            if (BRow* row = list_view_->RowAt(selected_index)) {
                list_view_->SetFocusRow(row, true);
            }
        }
        UpdateDetail();
    }

private:
    std::optional<std::size_t> SelectedIndex() const {
        const BRow* selected = list_view_->CurrentSelection();
        const int32 index = selected == nullptr ? -1 : list_view_->IndexOf(selected);
        if (index < 0 || static_cast<std::size_t>(index) >= entries_.size()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(index);
    }

    void UpdateDetail() {
        const auto index = SelectedIndex();
        if (!index) {
            detail_view_->SetText("No filter report entries.");
            return;
        }
        const auto& entry = entries_[*index];
        current_entry_id_ = entry.id;
        std::ostringstream detail;
        detail << "Timestamp: " << FormatTimestamp(entry.timestamp) << '\n'
               << "Mailbox: " << entry.mailbox_name << '\n'
               << "Sender: " << entry.sender << '\n'
               << "Subject: " << entry.subject << '\n'
               << "Matched Rules:\n" << JoinLines(entry.matched_rules);
        detail_view_->SetText(detail.str().c_str());
    }

    void OpenSelected() {
        const auto index = SelectedIndex();
        if (!index) {
            return;
        }
        const auto& entry = entries_[*index];
        if (!shell_host_.OpenMessageWindow(entry.mailbox_id, entry.message_id)) {
            BAlert("filter-report", "The referenced message is no longer available locally.", "OK")->Go();
        }
    }

    void ClearReport() {
        shell_host_.FilterReport().Clear();
        std::string ignored;
        shell_host_.FilterReport().SaveToFile(shell_host_.DataRootPath() / "FilterReport.ini", &ignored);
        current_entry_id_.clear();
        Refresh();
    }

    BColumnListView* list_view_ = nullptr;
    BTextView* detail_view_ = nullptr;
    std::vector<FilterReportEntry> entries_;
    std::string current_entry_id_;
};

class LinkHistoryTablePane final : public WazooPaneBase {
public:
    explicit LinkHistoryTablePane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "link-history") {
        list_view_ = new ContextColumnListView("link-history-list",
                                               new BMessage(kLinkHistorySelectionMessage),
                                               new BMessage(kLinkHistoryOpenMessage),
                                               kPaneDeleteMessage,
                                               [this](BPoint where) { ShowContextMenu(where); });
        list_view_->AddColumn(new BStringColumn("Type", 104.0f, 72.0f, 180.0f, B_TRUNCATE_END),
                              kLinkHistoryFieldType);
        list_view_->AddColumn(new BStringColumn("Title", 180.0f, 96.0f, 320.0f, B_TRUNCATE_END),
                              kLinkHistoryFieldTitle);
        list_view_->AddColumn(new BStringColumn("Target", 280.0f, 120.0f, 420.0f, B_TRUNCATE_END),
                              kLinkHistoryFieldTarget);
        list_view_->AddColumn(new BStringColumn("Source", 220.0f, 96.0f, 360.0f, B_TRUNCATE_END),
                              kLinkHistoryFieldSource);
        list_view_->AddColumn(new BStringColumn("Launched", 92.0f, 64.0f, 140.0f, B_TRUNCATE_END),
                              kLinkHistoryFieldLaunched);
        list_view_->AddColumn(new BStringColumn("Timestamp", 148.0f, 112.0f, 220.0f, B_TRUNCATE_END),
                              kLinkHistoryFieldTimestamp);
        RestoreColumnListState(*list_view_,
                               GuiPreferencesFromSettings(shell_host_.Settings()).link_history_column_layout);

        detail_view_ = new BTextView("link-history-detail");
        detail_view_->MakeEditable(false);
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);

        auto* open_button = new BButton("link-history-open", "Open", new BMessage(kLinkHistoryOpenMessage));
        auto* remove_button =
            new BButton("link-history-remove", "Remove", new BMessage(kLinkHistoryRemoveMessage));
        auto* clear_button = new BButton("link-history-clear", "Clear", new BMessage(kLinkHistoryClearMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(new BScrollView("link-history-scroll", list_view_, 0, false, true), 0.64f)
                .Add(new BScrollView("link-history-detail-scroll", detail_view_, 0, false, true), 0.36f)
            .End()
            .AddGroup(B_HORIZONTAL, 8)
                .Add(open_button)
                .Add(remove_button)
                .Add(clear_button)
                .AddGlue()
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
    }

    bool HandleCommand(uint32 command_id) override {
        switch (command_id) {
            case kLinkHistoryOpenMessage:
            case kPaneActionMessage:
                OpenSelected();
                return true;
            case kLinkHistoryRemoveMessage:
            case kPaneDeleteMessage:
                RemoveSelected();
                return true;
            case kLinkHistoryClearMessage:
                ClearHistory();
                return true;
            default:
                return false;
        }
    }

    bool IsCommandEnabled(uint32 command_id) const override {
        const bool has_selection = SelectedIndex().has_value();
        switch (command_id) {
            case kLinkHistoryOpenMessage:
            case kPaneActionMessage:
            case kPaneDeleteMessage:
            case kLinkHistoryRemoveMessage:
                return has_selection;
            case kLinkHistoryClearMessage:
                return !entries_.empty();
            default:
                return false;
        }
    }

    BView* PreferredFocusView() const override {
        return list_view_;
    }

    void PersistState() override {
        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        preferences.link_history_column_layout = SerializeColumnListState(*list_view_);
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kLinkHistorySelectionMessage:
                UpdateDetail();
                return;
            case kLinkHistoryOpenMessage:
                OpenSelected();
                return;
            case kLinkHistoryRemoveMessage:
                RemoveSelected();
                return;
            case kLinkHistoryClearMessage:
                ClearHistory();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        entries_ = shell_host_.LinkHistory().Entries();
        std::stable_sort(entries_.begin(), entries_.end(), [](const auto& left, const auto& right) {
            return left.timestamp > right.timestamp;
        });
        list_view_->Clear();
        int32 selected_index = -1;
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            auto* row = new BRow();
            row->SetField(new BStringField(LinkHistoryKindLabel(entries_[index].kind).c_str()),
                          kLinkHistoryFieldType);
            row->SetField(new BStringField(entries_[index].title.c_str()), kLinkHistoryFieldTitle);
            row->SetField(new BStringField(entries_[index].target.c_str()), kLinkHistoryFieldTarget);
            row->SetField(new BStringField(entries_[index].source_context.c_str()), kLinkHistoryFieldSource);
            row->SetField(new BStringField(entries_[index].launched ? "Yes" : "No"),
                          kLinkHistoryFieldLaunched);
            row->SetField(new BStringField(FormatTimestamp(entries_[index].timestamp).c_str()),
                          kLinkHistoryFieldTimestamp);
            list_view_->AddRow(row);
            if (!current_entry_id_.empty() && entries_[index].id == current_entry_id_) {
                selected_index = static_cast<int32>(index);
            }
        }
        if (selected_index < 0 && !entries_.empty()) {
            selected_index = 0;
        }
        if (selected_index >= 0) {
            if (BRow* row = list_view_->RowAt(selected_index)) {
                list_view_->SetFocusRow(row, true);
            }
        }
        UpdateDetail();
    }

private:
    std::optional<std::size_t> SelectedIndex() const {
        const BRow* selected = list_view_->CurrentSelection();
        const int32 index = selected == nullptr ? -1 : list_view_->IndexOf(selected);
        if (index < 0 || static_cast<std::size_t>(index) >= entries_.size()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(index);
    }

    void UpdateDetail() {
        const auto index = SelectedIndex();
        if (!index) {
            detail_view_->SetText("No link history entries.");
            return;
        }
        const auto& entry = entries_[*index];
        current_entry_id_ = entry.id;
        std::ostringstream detail;
        detail << "Type: " << LinkHistoryKindLabel(entry.kind) << '\n'
               << "Title: " << entry.title << '\n'
               << "Target: " << entry.target << '\n'
               << "Source: " << entry.source_context << '\n'
               << "Launched: " << (entry.launched ? "Yes" : "No") << '\n'
               << "Timestamp: " << FormatTimestamp(entry.timestamp);
        detail_view_->SetText(detail.str().c_str());
    }

    void OpenSelected() {
        const auto index = SelectedIndex();
        if (!index) {
            return;
        }
        const auto& entry = entries_[*index];
        bool opened = false;
        if (entry.kind == LinkHistoryKind::kUrl) {
            opened = LaunchExternalTarget(entry.target);
        } else {
            opened = LaunchPath(entry.target);
        }
        if (!opened) {
            BAlert("link-history", "Unable to open the selected history target.", "OK")->Go();
        }
    }

    void RemoveSelected() {
        const auto index = SelectedIndex();
        if (!index) {
            return;
        }
        shell_host_.LinkHistory().Remove(entries_[*index].id);
        std::string ignored;
        shell_host_.LinkHistory().SaveToFile(shell_host_.DataRootPath() / "LinkHistory.ini", &ignored);
        current_entry_id_.clear();
        Refresh();
    }

    void ClearHistory() {
        shell_host_.LinkHistory().Clear();
        std::string ignored;
        shell_host_.LinkHistory().SaveToFile(shell_host_.DataRootPath() / "LinkHistory.ini", &ignored);
        current_entry_id_.clear();
        Refresh();
    }

    void ShowContextMenu(BPoint where) {
        BPopUpMenu menu("link-history-context", false, false);
        auto* open_item = new BMenuItem("Open", new BMessage(kLinkHistoryOpenMessage));
        auto* remove_item = new BMenuItem("Remove", new BMessage(kLinkHistoryRemoveMessage));
        auto* clear_item = new BMenuItem("Clear", new BMessage(kLinkHistoryClearMessage));
        open_item->SetEnabled(IsCommandEnabled(kLinkHistoryOpenMessage));
        remove_item->SetEnabled(IsCommandEnabled(kLinkHistoryRemoveMessage));
        clear_item->SetEnabled(IsCommandEnabled(kLinkHistoryClearMessage));
        menu.AddItem(open_item);
        menu.AddItem(remove_item);
        menu.AddSeparatorItem();
        menu.AddItem(clear_item);
        menu.SetTargetForItems(this);
        list_view_->ConvertToScreen(&where);
        menu.Go(where, true, false, true);
    }

    ContextColumnListView* list_view_ = nullptr;
    BTextView* detail_view_ = nullptr;
    std::vector<LinkHistoryEntry> entries_;
    std::string current_entry_id_;
};

class DirectoryServicesManagerPane final : public WazooPaneBase {
public:
    explicit DirectoryServicesManagerPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "directory-services") {
        split_view_ = new BSplitView(B_HORIZONTAL);
        summary_view_ = new BStringView("directory-summary", "Directory Services");
        provider_list_ = new BListView("directory-providers");
        provider_list_->SetSelectionMode(B_MULTIPLE_SELECTION_LIST);
        provider_list_->SetSelectionMessage(new BMessage(kDirectoryProviderSelectionMessage));
        query_control_ = new BTextControl("directory-query", "Query", "", nullptr);
        query_control_->SetMessage(new BMessage(kDirectorySearchMessage));
        results_view_ = new BColumnListView("directory-results",
                                            B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS |
                                                B_FULL_UPDATE_ON_RESIZE);
        results_view_->SetSelectionMode(B_MULTIPLE_SELECTION_LIST);
        results_view_->SetSelectionMessage(new BMessage(kDirectoryResultSelectionMessage));
        results_view_->AddColumn(new BStringColumn("Name", 180.0f, 96.0f, 320.0f, B_TRUNCATE_END),
                                 kDirectoryResultFieldName);
        results_view_->AddColumn(new BStringColumn("Email", 220.0f, 120.0f, 360.0f, B_TRUNCATE_END),
                                 kDirectoryResultFieldEmail);
        results_view_->AddColumn(new BStringColumn("Provider", 140.0f, 96.0f, 240.0f, B_TRUNCATE_END),
                                 kDirectoryResultFieldProvider);
        detail_view_ = new BTextView("directory-detail");
        detail_view_->MakeEditable(false);
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);
        detail_view_->MakeSelectable(true);

        search_button_ = new BButton("directory-search", "Search", new BMessage(kDirectorySearchMessage));
        to_button_ = new BButton("directory-compose-to", "To", new BMessage(kDirectoryComposeToMessage));
        cc_button_ = new BButton("directory-compose-cc", "Cc", new BMessage(kDirectoryComposeCcMessage));
        bcc_button_ = new BButton("directory-compose-bcc", "Bcc", new BMessage(kDirectoryComposeBccMessage));
        nickname_button_ =
            new BButton("directory-nickname", "Add to Nicknames", new BMessage(kDirectoryNicknameMessage));
        copy_button_ = new BButton("directory-copy", "Copy Address", new BMessage(kDirectoryCopyMessage));
        keep_on_top_box_ =
            new BCheckBox("directory-keep-on-top", "Keep on Top", new BMessage(kDirectoryKeepOnTopMessage));

        auto* right_group = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(right_group, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(summary_view_)
            .Add(query_control_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(search_button_)
                .Add(to_button_)
                .Add(cc_button_)
                .Add(bcc_button_)
                .Add(nickname_button_)
                .Add(copy_button_)
                .Add(keep_on_top_box_)
                .AddGlue()
            .End()
            .Add(new BScrollView("directory-results-scroll", results_view_, 0, false, true))
            .Add(new BScrollView("directory-detail-scroll", detail_view_, 0, false, true), 0.34f);

        split_view_->AddChild(new BScrollView("directory-providers-scroll", provider_list_, 0, false, true),
                              0.26f);
        split_view_->AddChild(right_group, 0.74f);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(split_view_);
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        provider_list_->SetTarget(this);
        query_control_->SetTarget(this);
        if (query_control_->TextView() != nullptr) {
            query_control_->TextView()->SetModificationMessage(new BMessage(kDirectoryQueryModifiedMessage));
        }
        results_view_->SetTarget(this);
        search_button_->SetTarget(this);
        to_button_->SetTarget(this);
        cc_button_->SetTarget(this);
        bcc_button_->SetTarget(this);
        nickname_button_->SetTarget(this);
        copy_button_->SetTarget(this);
        keep_on_top_box_->SetTarget(this);
        if (tab_filter_ == nullptr) {
            tab_filter_ = new DirectoryTabFilter(this);
            if (Window() != nullptr) {
                Window()->AddCommonFilter(tab_filter_);
            }
        }
        ApplyKeepOnTop(keep_on_top_box_->Value() == B_CONTROL_ON);
        UpdateActionButtons();
        FocusQuery();
    }

    void PersistState() override {
        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        preferences.directory_services_split_layout = SerializeSplitWeights(*split_view_);
        preferences.directory_services_keep_on_top = keep_on_top_;
        preferences.directory_services_active_provider_ids = ActiveProviderIds();
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kDirectoryProviderSelectionMessage:
                PerformSearch();
                return;
            case kDirectoryResultSelectionMessage:
                UpdateSummary(query_control_->Text() != nullptr ? query_control_->Text() : "");
                UpdateDetail();
                return;
            case kDirectorySearchMessage:
                PerformSearch();
                return;
            case kDirectoryQueryModifiedMessage:
                results_.clear();
                results_view_->Clear();
                CheckClosePrintPreview();
                UpdateSummary(query_control_->Text() != nullptr ? query_control_->Text() : "");
                UpdateDetail();
                UpdateActionButtons();
                return;
            case kDirectoryComposeToMessage:
                ComposeToSelected("to");
                return;
            case kDirectoryComposeCcMessage:
                ComposeToSelected("cc");
                return;
            case kDirectoryComposeBccMessage:
                ComposeToSelected("bcc");
                return;
            case kDirectoryNicknameMessage:
                AddSelectedToNicknames();
                return;
            case kDirectoryCopyMessage:
                CopySelectedAddress();
                return;
            case kDirectoryKeepOnTopMessage:
                ApplyKeepOnTop(keep_on_top_box_ != nullptr && keep_on_top_box_->Value() == B_CONTROL_ON);
                PersistState();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        const auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        providers_ = shell_host_.DirectoryServices().Providers();
        provider_list_->MakeEmpty();
        const std::vector<std::string> preferred_provider_ids =
            preferences.directory_services_active_provider_ids.empty()
                ? [&]() {
                      std::vector<std::string> defaults;
                      defaults.reserve(providers_.size());
                      for (const auto& provider : providers_) {
                          defaults.push_back(provider.id);
                      }
                      return defaults;
                  }()
                : preferences.directory_services_active_provider_ids;
        for (std::size_t index = 0; index < providers_.size(); ++index) {
            provider_list_->AddItem(new BStringItem(providers_[index].display_name.c_str()));
            if (std::find(preferred_provider_ids.begin(),
                          preferred_provider_ids.end(),
                          providers_[index].id) != preferred_provider_ids.end()) {
                provider_list_->Select(static_cast<int32>(index), true);
            }
        }
        if (ActiveProviderIds().empty() && !providers_.empty()) {
            for (std::size_t index = 0; index < providers_.size(); ++index) {
                provider_list_->Select(static_cast<int32>(index), index != 0);
            }
        }
        RestoreSplitWeights(*split_view_,
                            preferences.directory_services_split_layout);
        if (keep_on_top_box_ != nullptr) {
            keep_on_top_box_->SetValue(preferences.directory_services_keep_on_top ? B_CONTROL_ON
                                                                                  : B_CONTROL_OFF);
        }
        ApplyKeepOnTop(preferences.directory_services_keep_on_top);
        if (const auto query = shell_host_.TakePendingDirectoryQuery()) {
            query_control_->SetText(query->c_str());
            if (query_control_->TextView() != nullptr) {
                query_control_->TextView()->SelectAll();
                query_control_->TextView()->MakeFocus(true);
            } else {
                query_control_->MakeFocus(true);
            }
        }
        PerformSearch();
    }

    BView* PreferredFocusView() const override {
        if (query_control_ != nullptr && query_control_->TextView() != nullptr) {
            return query_control_->TextView();
        }
        return query_control_;
    }

    bool CanPrintPreview() const override {
        return !printable_detail_cache_.empty();
    }

    bool CanDirectPrint() const override {
        return !printable_detail_cache_.empty() && HasPrinterCommand();
    }

    bool HandlePrint(bool preview) override {
        std::filesystem::path preview_path;
        std::filesystem::path printable_path;
        if (!EnsurePrintArtifacts(&preview_path, &printable_path)) {
            return false;
        }
        return preview ? LaunchPath(preview_path) : SendPathToPrinter(printable_path);
    }

private:
    class DirectoryTabFilter final : public BMessageFilter {
    public:
        explicit DirectoryTabFilter(DirectoryServicesManagerPane* owner)
            : BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE),
              owner_(owner) {}

        filter_result Filter(BMessage* message, BHandler** target) override {
            (void)target;
            if (owner_ == nullptr || message == nullptr || message->what != B_KEY_DOWN) {
                return B_DISPATCH_MESSAGE;
            }
            const char* bytes = nullptr;
            if (message->FindString("bytes", &bytes) != B_OK || bytes == nullptr || bytes[0] != B_TAB) {
                return B_DISPATCH_MESSAGE;
            }
            int32 modifiers = 0;
            (void)message->FindInt32("modifiers", &modifiers);
            if (owner_->HandleTabNavigation((modifiers & B_SHIFT_KEY) != 0)) {
                return B_SKIP_MESSAGE;
            }
            return B_DISPATCH_MESSAGE;
        }

    private:
        DirectoryServicesManagerPane* owner_ = nullptr;
    };

    BTextView* QueryTextView() const {
        return query_control_ != nullptr ? query_control_->TextView() : nullptr;
    }

    std::vector<std::size_t> SelectedProviderIndices() const {
        std::vector<std::size_t> indices;
        if (provider_list_ == nullptr) {
            return indices;
        }
        for (int32 ordinal = 0;; ++ordinal) {
            const int32 selection = provider_list_->CurrentSelection(ordinal);
            if (selection < 0) {
                break;
            }
            if (static_cast<std::size_t>(selection) < providers_.size()) {
                indices.push_back(static_cast<std::size_t>(selection));
            }
        }
        return indices;
    }

    std::vector<std::string> ActiveProviderIds() const {
        std::vector<std::string> ids;
        for (const auto index : SelectedProviderIndices()) {
            ids.push_back(providers_[index].id);
        }
        return ids;
    }

    bool HasActiveProviders() const {
        return !ActiveProviderIds().empty();
    }

    std::optional<std::size_t> SelectedResultIndex() const {
        const BRow* selected = results_view_->CurrentSelection();
        const int32 index = selected == nullptr ? -1 : results_view_->IndexOf(selected);
        if (index < 0 || static_cast<std::size_t>(index) >= results_.size()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(index);
    }

    std::vector<std::size_t> SelectedResultIndices() const {
        std::vector<std::size_t> indices;
        BRow* selected = nullptr;
        while ((selected = results_view_->CurrentSelection(selected)) != nullptr) {
            const int32 index = results_view_->IndexOf(selected);
            if (index >= 0 && static_cast<std::size_t>(index) < results_.size()) {
                indices.push_back(static_cast<std::size_t>(index));
            }
        }
        return indices;
    }

    std::vector<DirectoryEntry> SelectedResults() const {
        std::vector<DirectoryEntry> selected_results;
        for (const auto index : SelectedResultIndices()) {
            selected_results.push_back(results_[index]);
        }
        return selected_results;
    }

    bool HandleTabNavigation(bool shift) {
        if (Window() == nullptr) {
            return false;
        }
        BView* focus = Window()->CurrentFocus();
        if (focus == nullptr) {
            return false;
        }

        if (focus == QueryTextView()) {
            return shift ? FocusKeepOnTop() : FocusResults();
        }
        if (focus == results_view_) {
            return shift ? FocusQuery() : FocusDetail();
        }
        if (focus == detail_view_) {
            return shift ? FocusResults() : FocusAction(0, 1);
        }

        const std::array<BView*, 6> actions = {
            to_button_, cc_button_, bcc_button_, nickname_button_, copy_button_, keep_on_top_box_};
        for (std::size_t index = 0; index < actions.size(); ++index) {
            if (focus != actions[index]) {
                continue;
            }
            if (shift) {
                return index == 0 ? FocusDetail() : FocusAction(static_cast<int>(index) - 1, -1);
            }
            return index + 1 >= actions.size() ? FocusQuery()
                                               : FocusAction(static_cast<int>(index) + 1, 1);
        }
        return false;
    }

    bool FocusQuery() {
        if (BTextView* query_text = QueryTextView()) {
            query_text->MakeFocus(true);
            return true;
        }
        if (query_control_ != nullptr) {
            query_control_->MakeFocus(true);
            return true;
        }
        return false;
    }

    bool FocusResults() {
        if (results_view_ == nullptr) {
            return false;
        }
        if (results_view_->CurrentSelection() == nullptr && results_view_->CountRows() > 0) {
            if (BRow* first = results_view_->RowAt(0)) {
                results_view_->SetFocusRow(first, true);
            }
        }
        results_view_->MakeFocus(true);
        return true;
    }

    bool FocusDetail() {
        if (detail_view_ == nullptr) {
            return false;
        }
        detail_view_->MakeFocus(true);
        return true;
    }

    bool FocusKeepOnTop() {
        if (keep_on_top_box_ == nullptr) {
            return false;
        }
        keep_on_top_box_->MakeFocus(true);
        return true;
    }

    bool FocusAction(int start_index, int step) {
        const std::array<BControl*, 6> controls = {
            to_button_, cc_button_, bcc_button_, nickname_button_, copy_button_, keep_on_top_box_};
        for (int index = start_index; index >= 0 && index < static_cast<int>(controls.size()); index += step) {
            BControl* control = controls[static_cast<std::size_t>(index)];
            if (control != nullptr && control->IsEnabled()) {
                control->MakeFocus(true);
                return true;
            }
        }
        return step > 0 ? FocusQuery() : FocusDetail();
    }

    void UpdateActionButtons() {
        const bool can_search = query_control_ != nullptr && query_control_->Text() != nullptr &&
                                query_control_->Text()[0] != '\0' && HasActiveProviders();
        const bool has_result = !SelectedResultIndices().empty();
        if (search_button_ != nullptr) {
            search_button_->SetEnabled(can_search);
        }
        if (to_button_ != nullptr) {
            to_button_->SetEnabled(has_result);
        }
        if (cc_button_ != nullptr) {
            cc_button_->SetEnabled(has_result);
        }
        if (bcc_button_ != nullptr) {
            bcc_button_->SetEnabled(has_result);
        }
        if (nickname_button_ != nullptr) {
            nickname_button_->SetEnabled(has_result);
        }
        if (copy_button_ != nullptr) {
            copy_button_->SetEnabled(has_result);
        }
    }

    void ApplyKeepOnTop(bool enabled) {
        keep_on_top_ = enabled;
        if (keep_on_top_box_ != nullptr &&
            keep_on_top_box_->Value() != (enabled ? B_CONTROL_ON : B_CONTROL_OFF)) {
            keep_on_top_box_->SetValue(enabled ? B_CONTROL_ON : B_CONTROL_OFF);
        }
        if (Window() != nullptr) {
            Window()->SetFeel(enabled ? B_FLOATING_APP_WINDOW_FEEL : B_NORMAL_WINDOW_FEEL);
        }
    }

    void CheckClosePrintPreview() {
        printable_detail_cache_.clear();
        const auto render_directory = shell_host_.DataRootPath() / "Cache" / "DirectoryServices";
        std::error_code ignored;
        std::filesystem::remove(render_directory / "print-preview.html", ignored);
        std::filesystem::remove(render_directory / "print-directory.txt", ignored);
    }

    void PerformSearch() {
        const auto active_provider_ids = ActiveProviderIds();
        const std::string query = query_control_->Text() != nullptr ? query_control_->Text() : "";
        CheckClosePrintPreview();
        results_.clear();
        if (!query.empty() && !active_provider_ids.empty()) {
            results_ = shell_host_.DirectoryServices().SearchProviders(active_provider_ids, query);
        }

        results_view_->Clear();
        for (const auto& result : results_) {
            auto* row = new BRow();
            const auto address_count = result.email_addresses.empty() ? std::size_t{0}
                                                                      : result.email_addresses.size();
            const std::string display_name =
                result.display_name.empty() ? result.email_address : result.display_name;
            std::string email_summary = result.email_address;
            if (address_count > 1) {
                email_summary += " (+" + std::to_string(address_count - 1) + ')';
            }
            const auto provider_it =
                std::find_if(providers_.begin(), providers_.end(), [&](const auto& provider) {
                    return provider.id == result.provider_id;
                });
            const std::string provider_label =
                provider_it == providers_.end() ? result.provider_id : provider_it->display_name;
            row->SetField(new BStringField(display_name.c_str()),
                          kDirectoryResultFieldName);
            row->SetField(new BStringField(email_summary.c_str()), kDirectoryResultFieldEmail);
            row->SetField(new BStringField(provider_label.c_str()), kDirectoryResultFieldProvider);
            results_view_->AddRow(row);
        }
        if (!results_.empty()) {
            if (BRow* row = results_view_->RowAt(0)) {
                results_view_->SetFocusRow(row, true);
            }
        }
        UpdateSummary(query);
        UpdateDetail();
        UpdateActionButtons();
        PersistState();
    }

    void UpdateDetail() {
        CheckClosePrintPreview();
        const std::string query = query_control_->Text() != nullptr ? query_control_->Text() : "";
        const auto active_provider_ids = ActiveProviderIds();
        const auto selection = SelectedResults();
        if (selection.empty()) {
            printable_detail_cache_ =
                shell_host_.DirectoryServices().PrintableText(active_provider_ids, query, results_);
            detail_view_->SetText(shell_host_.DirectoryServices()
                                      .LongDetailText(active_provider_ids, query, results_)
                                      .c_str());
            UpdateActionButtons();
            return;
        }
        printable_detail_cache_ = shell_host_.DirectoryServices().PrintableDetailText(selection);
        detail_view_->SetText(shell_host_.DirectoryServices().LongDetailText(selection).c_str());
        UpdateActionButtons();
    }

    void ComposeToSelected(std::string_view header_field) {
        const auto selection = SelectedResults();
        if (selection.empty()) {
            return;
        }
        ComposeMessage message;
        const std::string address = shell_host_.DirectoryServices().ComposeAddressText(selection);
        if (header_field == "to") {
            message.headers.to = address;
        } else if (header_field == "cc") {
            message.headers.cc = address;
        } else if (header_field == "bcc") {
            message.headers.bcc = address;
        } else {
            return;
        }
        shell_host_.OpenComposer(message);
    }

    void AddSelectedToNicknames() {
        const auto selection = SelectedResults();
        if (selection.empty()) {
            return;
        }
        NicknameEntry entry;
        if (selection.size() == 1) {
            entry.nickname = selection.front().display_name.empty() ? selection.front().email_address
                                                                    : selection.front().display_name;
            entry.full_name = selection.front().display_name;
            entry.notes = selection.front().notes;
        } else {
            entry.nickname = query_control_->Text() != nullptr && query_control_->Text()[0] != '\0'
                                 ? query_control_->Text()
                                 : "Directory Results";
            entry.full_name = "Directory Results";
            entry.notes = "Imported from Directory Services.";
        }
        for (const auto& result : selection) {
            const auto source_addresses =
                result.email_addresses.empty() ? std::vector<std::string>{result.email_address}
                                               : result.email_addresses;
            for (const auto& address : source_addresses) {
                if (!address.empty() &&
                    std::find(entry.addresses.begin(), entry.addresses.end(), address) ==
                        entry.addresses.end()) {
                    entry.addresses.push_back(address);
                }
            }
        }
        entry.recipient_list = true;
        shell_host_.Nicknames().AddOrReplace(entry);
        std::string error_message;
        shell_host_.Nicknames().SaveToFile(shell_host_.DataRootPath() / "Nicknames.txt", &error_message);
        shell_host_.ReloadWorkspace();
    }

    void CopySelectedAddress() {
        const auto selection = SelectedResults();
        if (selection.empty()) {
            return;
        }
        CopyTextToClipboard(shell_host_.DirectoryServices().ComposeAddressText(selection));
    }

    bool EnsurePrintArtifacts(std::filesystem::path* preview_path,
                              std::filesystem::path* printable_path) const {
        if (printable_detail_cache_.empty()) {
            return false;
        }
        const auto render_directory = shell_host_.DataRootPath() / "Cache" / "DirectoryServices";
        std::error_code mkdir_error;
        std::filesystem::create_directories(render_directory, mkdir_error);
        if (mkdir_error) {
            return false;
        }

        const auto preview_document_path = render_directory / "print-preview.html";
        const auto print_path = render_directory / "print-directory.txt";
        const std::string preview_document =
            "<!doctype html><html><head><meta charset=\"utf-8\"><title>Directory Services Print Preview</title>"
            "<style>body{font-family:monospace;margin:24px;line-height:1.4;white-space:pre-wrap;}"
            "pre{white-space:pre-wrap;word-break:break-word;}</style></head><body><pre>" +
            EscapeHtmlText(printable_detail_cache_) + "</pre></body></html>";
        if (!WriteWholeFile(preview_document_path, preview_document) ||
            !WriteWholeFile(print_path, printable_detail_cache_)) {
            return false;
        }

        if (preview_path != nullptr) {
            *preview_path = preview_document_path;
        }
        if (printable_path != nullptr) {
            *printable_path = print_path;
        }
        return true;
    }

    void UpdateSummary(std::string_view query) {
        const auto active_provider_ids = ActiveProviderIds();
        const std::string provider_name = [&]() -> std::string {
            if (active_provider_ids.empty()) {
                return "Directory Services";
            }
            std::ostringstream names;
            for (std::size_t index = 0; index < providers_.size(); ++index) {
                if (std::find(active_provider_ids.begin(),
                              active_provider_ids.end(),
                              providers_[index].id) == active_provider_ids.end()) {
                    continue;
                }
                if (names.tellp() > 0) {
                    names << ", ";
                }
                names << providers_[index].display_name;
            }
            return names.str().empty() ? std::string("Directory Services") : names.str();
        }();

        std::ostringstream summary;
        summary << provider_name;
        if (!query.empty()) {
            summary << "  •  Query: " << query;
            summary << "  •  Results: " << results_.size();
        }
        const auto selected_results = SelectedResultIndices();
        if (!selected_results.empty()) {
            summary << "  •  Selected: " << selected_results.size();
        }
        summary_view_->SetText(summary.str().c_str());
        if (Window() != nullptr) {
            std::string title = "Tools - Directory Services";
            if (provider_name != "Directory Services") {
                title += " (" + provider_name + ")";
            }
            if (!query.empty()) {
                title += ": " + std::string(query);
            }
            Window()->SetTitle(title.c_str());
        }
    }

        BSplitView* split_view_ = nullptr;
    BStringView* summary_view_ = nullptr;
    BListView* provider_list_ = nullptr;
    BTextControl* query_control_ = nullptr;
    BColumnListView* results_view_ = nullptr;
    BTextView* detail_view_ = nullptr;
    BButton* search_button_ = nullptr;
    BButton* to_button_ = nullptr;
    BButton* cc_button_ = nullptr;
    BButton* bcc_button_ = nullptr;
    BButton* nickname_button_ = nullptr;
    BButton* copy_button_ = nullptr;
    BCheckBox* keep_on_top_box_ = nullptr;
    DirectoryTabFilter* tab_filter_ = nullptr;
    std::vector<DirectoryProviderInfo> providers_;
    std::vector<DirectoryEntry> results_;
    std::string printable_detail_cache_;
    bool keep_on_top_ = false;
};

class FileBrowserManagerPane final : public WazooPaneBase {
public:
    explicit FileBrowserManagerPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "file-browser") {
        split_view_ = new BSplitView(B_HORIZONTAL);
        list_view_ = new BOutlineListView("file-browser-tree");
        list_view_->SetSelectionMessage(new BMessage(kFileBrowserSelectionMessage));
        list_view_->SetInvocationMessage(new BMessage(kFileBrowserOpenMessage));
        detail_view_ = new BTextView("file-browser-detail");
        detail_view_->MakeEditable(false);
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);

        split_view_->AddChild(new BScrollView("file-browser-tree-scroll", list_view_, 0, false, true), 0.38f);
        split_view_->AddChild(new BScrollView("file-browser-detail-scroll", detail_view_, 0, false, true), 0.62f);

        auto* open_button = new BButton("file-browser-open", "Open", new BMessage(kFileBrowserOpenMessage));
        auto* reveal_button =
            new BButton("file-browser-reveal", "Reveal", new BMessage(kFileBrowserRevealMessage));
        auto* refresh_button =
            new BButton("file-browser-refresh", "Refresh", new BMessage(kFileBrowserRefreshMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(split_view_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(open_button)
                .Add(reveal_button)
                .Add(refresh_button)
                .AddGlue()
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
    }

    void PersistState() override {
        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        preferences.file_browser_split_layout = SerializeSplitWeights(*split_view_);
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kFileBrowserSelectionMessage:
                UpdateDetail();
                return;
            case kFileBrowserOpenMessage:
                OpenSelected();
                return;
            case kFileBrowserRevealMessage:
                RevealSelected();
                return;
            case kFileBrowserRefreshMessage:
                Refresh();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        struct RootEntry {
            std::string label;
            std::filesystem::path path;
        };

        const auto data_root = shell_host_.DataRootPath();
        const std::vector<RootEntry> roots = {
            {"Mail", data_root / "mailboxes"},
            {"Attachments", data_root / "Attachments"},
            {"Signatures", shell_host_.Signatures().RootDirectory()},
            {"Stationery", shell_host_.Stationery().RootDirectory()},
            {"Settings & Support", shell_host_.SettingsFilePath().parent_path()},
        };

        items_.clear();
        list_view_->MakeEmpty();
        int32 selected_index = -1;
        for (const auto& root : roots) {
            items_.push_back({root.label, root.path, true});
            list_view_->AddItem(new BStringItem(root.label.c_str(), 0, false));
            if (!current_path_.empty() && root.path == current_path_) {
                selected_index = static_cast<int32>(items_.size() - 1);
            }
            if (!std::filesystem::exists(root.path)) {
                continue;
            }
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root.path)) {
                std::error_code relative_error;
                const auto relative = std::filesystem::relative(entry.path(), root.path, relative_error);
                int32 depth = 1;
                if (!relative_error) {
                    depth += std::max<int32>(
                        0,
                        static_cast<int32>(std::distance(relative.begin(), relative.end())) - 1);
                }
                items_.push_back({entry.path().filename().string(), entry.path(), false});
                list_view_->AddItem(new BStringItem(entry.path().filename().string().c_str(), depth, false));
                if (!current_path_.empty() && entry.path() == current_path_) {
                    selected_index = static_cast<int32>(items_.size() - 1);
                }
            }
        }
        if (selected_index < 0 && !items_.empty()) {
            selected_index = 0;
        }
        if (selected_index >= 0) {
            list_view_->Select(selected_index);
        }
        RestoreSplitWeights(*split_view_, GuiPreferencesFromSettings(shell_host_.Settings()).file_browser_split_layout);
        UpdateDetail();
    }

private:
    struct ItemRecord {
        std::string label;
        std::filesystem::path path;
        bool root_group = false;
    };

    std::optional<ItemRecord> SelectedItem() const {
        const int32 index = list_view_->CurrentSelection();
        if (index < 0 || static_cast<std::size_t>(index) >= items_.size()) {
            return std::nullopt;
        }
        return items_[static_cast<std::size_t>(index)];
    }

    void UpdateDetail() {
        const auto selected = SelectedItem();
        if (!selected) {
            detail_view_->SetText("Select a file or shell root.");
            return;
        }
        current_path_ = selected->path;
        std::ostringstream detail;
        detail << "Label: " << selected->label << '\n'
               << "Path: " << selected->path.string() << '\n'
               << "Type: ";
        if (!std::filesystem::exists(selected->path)) {
            detail << "Unavailable";
        } else if (std::filesystem::is_directory(selected->path)) {
            detail << "Directory";
        } else if (std::filesystem::is_regular_file(selected->path)) {
            detail << "File";
        } else {
            detail << "Other";
        }
        if (std::filesystem::exists(selected->path) && std::filesystem::is_regular_file(selected->path)) {
            std::error_code size_error;
            const auto size = std::filesystem::file_size(selected->path, size_error);
            if (!size_error) {
                detail << '\n' << "Size: " << FormatFileSize(size);
            }
        }
        detail_view_->SetText(detail.str().c_str());
    }

    void OpenSelected() {
        const auto selected = SelectedItem();
        if (!selected || !std::filesystem::exists(selected->path)) {
            return;
        }
        if (!LaunchPath(selected->path)) {
            BAlert("file-browser", "Unable to open the selected path.", "OK")->Go();
        }
    }

    void RevealSelected() {
        const auto selected = SelectedItem();
        if (!selected || !std::filesystem::exists(selected->path)) {
            return;
        }
        const auto reveal_path =
            std::filesystem::is_directory(selected->path) ? selected->path : selected->path.parent_path();
        if (!LaunchPath(reveal_path)) {
            BAlert("file-browser", "Unable to reveal the selected path.", "OK")->Go();
        }
    }

    BSplitView* split_view_ = nullptr;
    BOutlineListView* list_view_ = nullptr;
    BTextView* detail_view_ = nullptr;
    std::vector<ItemRecord> items_;
    std::filesystem::path current_path_;
};

class SearchManagerPane final : public WazooPaneBase {
public:
    explicit SearchManagerPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "search") {
        query_control_ = new BTextControl("search-query", "Find", "", nullptr);
        scope_view_ = new BStringView("search-scope", "Scope: All mailboxes");
        subject_box_ = new BCheckBox("search-subject", "Subject", nullptr);
        headers_box_ = new BCheckBox("search-headers", "Headers", nullptr);
        body_box_ = new BCheckBox("search-body", "Body", nullptr);
        subject_box_->SetValue(B_CONTROL_ON);
        headers_box_->SetValue(B_CONTROL_ON);
        body_box_->SetValue(B_CONTROL_ON);

        auto* search_button = new BButton("search-run", "Search", new BMessage(kSearchRunMessage));
        auto* open_button = new BButton("search-open", "Open", new BMessage(kSearchOpenMessage));
        auto* clear_scope_button =
            new BButton("search-clear-scope", "Clear Scope", new BMessage(kSearchClearScopeMessage));

        results_view_ = new BColumnListView("search-results",
                                            B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS |
                                                B_FULL_UPDATE_ON_RESIZE);
        results_view_->SetSelectionMode(B_SINGLE_SELECTION_LIST);
        results_view_->SetSelectionMessage(new BMessage(kSearchSelectionMessage));
        results_view_->SetInvocationMessage(new BMessage(kSearchOpenMessage));
        results_view_->AddColumn(new BStringColumn("Mailbox", 180.0f, 96.0f, 320.0f, B_TRUNCATE_END),
                                 kSearchFieldMailbox);
        results_view_->AddColumn(new BStringColumn("Subject", 260.0f, 120.0f, 420.0f, B_TRUNCATE_END),
                                 kSearchFieldSubject);
        results_view_->AddColumn(new BStringColumn("Sender", 220.0f, 120.0f, 360.0f, B_TRUNCATE_END),
                                 kSearchFieldSender);
        results_view_->AddColumn(new BStringColumn("Score", 92.0f, 64.0f, 160.0f, B_TRUNCATE_END),
                                 kSearchFieldScore);
        RestoreColumnListState(*results_view_,
                               GuiPreferencesFromSettings(shell_host_.Settings()).search_column_layout);

        detail_view_ = new BTextView("search-detail");
        detail_view_->MakeEditable(false);
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);

        split_view_ = new BSplitView(B_HORIZONTAL);
        split_view_->AddChild(new BScrollView("search-results-scroll", results_view_, 0, false, true), 0.66f);
        split_view_->AddChild(new BScrollView("search-detail-scroll", detail_view_, 0, false, true), 0.34f);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(query_control_)
            .Add(scope_view_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(subject_box_)
                .Add(headers_box_)
                .Add(body_box_)
                .AddGlue()
                .Add(clear_scope_button)
                .Add(search_button)
                .Add(open_button)
            .End()
            .Add(split_view_);
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        results_view_->SetTarget(this);
    }

    void PersistState() override {
        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        preferences.search_column_layout = SerializeColumnListState(*results_view_);
        preferences.search_split_layout = SerializeSplitWeights(*split_view_);
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kSearchRunMessage:
                PerformSearch();
                return;
            case kSearchSelectionMessage:
                UpdateDetail();
                return;
            case kSearchOpenMessage:
                OpenSelected();
                return;
            case kSearchClearScopeMessage:
                search_scope_ = HaikuShellHost::SearchRequest::Scope::kAllMailboxes;
                anchor_mailbox_id_.clear();
                UpdateScopeLabel();
                PerformSearch();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        if (const auto request = shell_host_.TakePendingSearch()) {
            if (query_control_->Text() != nullptr) {
                query_control_->SetText(request->term.c_str());
            }
            search_scope_ = request->scope;
            anchor_mailbox_id_ = request->anchor_mailbox_id;
        }
        UpdateScopeLabel();
        RestoreSplitWeights(*split_view_, GuiPreferencesFromSettings(shell_host_.Settings()).search_split_layout);
        PerformSearch();
    }

private:
    struct ResultRecord {
        SearchHit hit;
        std::string mailbox_name;
        std::string subject;
        std::string sender;
    };

    std::optional<std::size_t> SelectedIndex() const {
        const BRow* selected = results_view_->CurrentSelection();
        const int32 index = selected == nullptr ? -1 : results_view_->IndexOf(selected);
        if (index < 0 || static_cast<std::size_t>(index) >= results_.size()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(index);
    }

    std::optional<MailboxRecord> AnchorMailbox() const {
        if (anchor_mailbox_id_.empty()) {
            return std::nullopt;
        }
        return shell_host_.Mailboxes().GetMailbox(anchor_mailbox_id_);
    }

    std::optional<MailboxRecord> CurrentFolderRoot() const {
        const auto anchor = AnchorMailbox();
        if (!anchor) {
            return std::nullopt;
        }
        if (!anchor->parent_id.empty()) {
            return shell_host_.Mailboxes().GetMailbox(anchor->parent_id);
        }
        return anchor;
    }

    bool MailboxMatchesFolderScope(const MailboxRecord& mailbox,
                                   const std::map<std::string, MailboxRecord>& mailboxes_by_id) const {
        const auto root = CurrentFolderRoot();
        if (!root) {
            return true;
        }
        if (mailbox.id == root->id) {
            return true;
        }

        std::string parent_id = mailbox.parent_id;
        while (!parent_id.empty()) {
            if (parent_id == root->id) {
                return true;
            }
            const auto parent = mailboxes_by_id.find(parent_id);
            if (parent == mailboxes_by_id.end()) {
                break;
            }
            parent_id = parent->second.parent_id;
        }
        return false;
    }

    void PerformSearch() {
        results_.clear();
        results_view_->Clear();

        SearchQuery query;
        query.term = query_control_->Text() != nullptr ? query_control_->Text() : "";
        query.search_subject = subject_box_->Value() == B_CONTROL_ON;
        query.search_headers = headers_box_->Value() == B_CONTROL_ON;
        query.search_body = body_box_->Value() == B_CONTROL_ON;
        if (query.term.empty()) {
            detail_view_->SetText("Enter a search term to look through local and cached mail.");
            return;
        }

        std::vector<MessageRecord> all_messages;
        std::map<std::string, std::string> mailbox_names;
        const auto mailboxes = shell_host_.Mailboxes().ListMailboxes();
        std::map<std::string, MailboxRecord> mailboxes_by_id;
        for (const auto& mailbox : mailboxes) {
            mailboxes_by_id.emplace(mailbox.id, mailbox);
        }
        for (const auto& mailbox : mailboxes) {
            if (search_scope_ == HaikuShellHost::SearchRequest::Scope::kCurrentMailbox &&
                mailbox.id != anchor_mailbox_id_) {
                continue;
            }
            if (search_scope_ == HaikuShellHost::SearchRequest::Scope::kCurrentFolder &&
                !MailboxMatchesFolderScope(mailbox, mailboxes_by_id)) {
                continue;
            }
            mailbox_names[mailbox.id] = mailbox.display_name;
            const auto mailbox_messages = shell_host_.Messages().ListMessages(mailbox.id);
            all_messages.insert(all_messages.end(), mailbox_messages.begin(), mailbox_messages.end());
        }

        auto hits = shell_host_.Search().Search(all_messages, query);
        for (const auto& hit : hits) {
            const auto message = shell_host_.Messages().GetMessage(hit.mailbox_id, hit.message_id);
            if (!message) {
                continue;
            }
            results_.push_back({hit,
                                mailbox_names[hit.mailbox_id],
                                message->subject.empty() ? "(No subject)" : message->subject,
                                message->sender});
        }

        int32 selected_index = -1;
        for (std::size_t index = 0; index < results_.size(); ++index) {
            auto* row = new BRow();
            row->SetField(new BStringField(results_[index].mailbox_name.c_str()), kSearchFieldMailbox);
            row->SetField(new BStringField(results_[index].subject.c_str()), kSearchFieldSubject);
            row->SetField(new BStringField(results_[index].sender.c_str()), kSearchFieldSender);
            row->SetField(new BStringField(std::to_string(results_[index].hit.score).c_str()),
                          kSearchFieldScore);
            results_view_->AddRow(row);
            if (!current_message_id_.empty() && results_[index].hit.message_id == current_message_id_) {
                selected_index = static_cast<int32>(index);
            }
        }

        if (selected_index < 0 && !results_.empty()) {
            selected_index = 0;
        }
        if (selected_index >= 0) {
            if (BRow* row = results_view_->RowAt(selected_index)) {
                results_view_->SetFocusRow(row, true);
            }
        }
        UpdateDetail();
    }

    void UpdateScopeLabel() {
        switch (search_scope_) {
            case HaikuShellHost::SearchRequest::Scope::kAllMailboxes:
                scope_view_->SetText("Scope: All mailboxes");
                return;
            case HaikuShellHost::SearchRequest::Scope::kCurrentMailbox: {
                const auto mailbox = AnchorMailbox();
                scope_view_->SetText(
                    ("Scope: Current mailbox (" +
                     (mailbox ? mailbox->display_name
                              : (anchor_mailbox_id_.empty() ? std::string("none") : anchor_mailbox_id_)) +
                     ")")
                        .c_str());
                return;
            }
            case HaikuShellHost::SearchRequest::Scope::kCurrentFolder: {
                const auto root = CurrentFolderRoot();
                if (root && AnchorMailbox() && root->id != AnchorMailbox()->id) {
                    scope_view_->SetText(
                        ("Scope: Current folder (" + root->display_name + ")").c_str());
                    return;
                }
                const auto mailbox = AnchorMailbox();
                scope_view_->SetText(
                    ("Scope: Current mailbox (" +
                     (mailbox ? mailbox->display_name
                              : (anchor_mailbox_id_.empty() ? std::string("none") : anchor_mailbox_id_)) +
                     ")")
                        .c_str());
                return;
            }
        }
    }

    void UpdateDetail() {
        const auto index = SelectedIndex();
        if (!index) {
            detail_view_->SetText(results_.empty() ? "No messages matched the current search."
                                                   : "Select a result to see the match details.");
            return;
        }
        const auto& result = results_[*index];
        current_message_id_ = result.hit.message_id;
        std::ostringstream detail;
        detail << "Mailbox: " << result.mailbox_name << '\n'
               << "Subject: " << result.subject << '\n'
               << "Sender: " << result.sender << '\n'
               << "Score: " << result.hit.score << '\n'
               << "Snippet: " << result.hit.snippet;
        detail_view_->SetText(detail.str().c_str());
    }

    void OpenSelected() {
        const auto index = SelectedIndex();
        if (!index) {
            return;
        }
        const auto& result = results_[*index];
        if (!shell_host_.OpenMessageWindow(result.hit.mailbox_id, result.hit.message_id)) {
            BAlert("search-results", "The selected message is no longer available locally.", "OK")->Go();
        }
    }

    BTextControl* query_control_ = nullptr;
    BStringView* scope_view_ = nullptr;
    BCheckBox* subject_box_ = nullptr;
    BCheckBox* headers_box_ = nullptr;
    BCheckBox* body_box_ = nullptr;
    BSplitView* split_view_ = nullptr;
    BColumnListView* results_view_ = nullptr;
    BTextView* detail_view_ = nullptr;
    std::vector<ResultRecord> results_;
    std::string current_message_id_;
    HaikuShellHost::SearchRequest::Scope search_scope_ =
        HaikuShellHost::SearchRequest::Scope::kAllMailboxes;
    std::string anchor_mailbox_id_;
};

class PluginManagerPane final : public WazooPaneBase {
public:
    explicit PluginManagerPane(HaikuShellHost& shell_host)
        : WazooPaneBase(shell_host, "plugins") {
        roots_view_ = new BTextView("plugins-roots");
        roots_view_->MakeEditable(false);
        roots_view_->SetWordWrap(true);
        roots_view_->SetInsets(8, 8, 8, 8);

        list_view_ = new BColumnListView("plugins-list",
                                         B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS |
                                             B_FULL_UPDATE_ON_RESIZE);
        list_view_->SetSelectionMode(B_SINGLE_SELECTION_LIST);
        list_view_->SetSelectionMessage(new BMessage(kPluginSelectionMessage));
        list_view_->AddColumn(new BStringColumn("Plugin", 220.0f, 120.0f, 360.0f, B_TRUNCATE_END),
                              kPluginFieldName);
        list_view_->AddColumn(new BStringColumn("Version", 104.0f, 72.0f, 180.0f, B_TRUNCATE_END),
                              kPluginFieldVersion);
        list_view_->AddColumn(new BStringColumn("Source Root", 140.0f, 96.0f, 240.0f, B_TRUNCATE_END),
                              kPluginFieldSourceRoot);
        list_view_->AddColumn(new BStringColumn("Path", 280.0f, 160.0f, 520.0f, B_TRUNCATE_MIDDLE),
                              kPluginFieldPath);
        list_view_->AddColumn(new BStringColumn("Capabilities", 140.0f, 96.0f, 240.0f, B_TRUNCATE_END),
                              kPluginFieldCapabilities);
        RestoreColumnListState(*list_view_, GuiPreferencesFromSettings(shell_host_.Settings()).plugin_column_layout);

        detail_view_ = new BTextView("plugins-detail");
        detail_view_->MakeEditable(false);
        detail_view_->SetWordWrap(true);
        detail_view_->SetInsets(8, 8, 8, 8);

        auto* rescan_button = new BButton("plugins-rescan", "Rescan", new BMessage(kPluginRescanMessage));
        auto* reveal_user_button =
            new BButton("plugins-reveal-user", "Reveal User Plugins", new BMessage(kPluginRevealUserRootMessage));
        auto* reveal_app_button =
            new BButton("plugins-reveal-app", "Reveal App Plugins", new BMessage(kPluginRevealAppRootMessage));
        auto* reveal_selected_button =
            new BButton("plugins-reveal-selected", "Reveal Selected", new BMessage(kPluginRevealSelectedMessage));

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(new BScrollView("plugins-roots-scroll", roots_view_, 0, false, true), 0.22f)
            .Add(new BScrollView("plugins-list-scroll", list_view_, 0, false, true), 0.46f)
            .Add(new BScrollView("plugins-detail-scroll", detail_view_, 0, false, true), 0.32f)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(rescan_button)
                .Add(reveal_user_button)
                .Add(reveal_app_button)
                .Add(reveal_selected_button)
                .AddGlue()
            .End();
        Refresh();
    }

    void AttachedToWindow() override {
        BGroupView::AttachedToWindow();
        list_view_->SetTarget(this);
    }

    void PersistState() override {
        auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
        preferences.plugin_column_layout = SerializeColumnListState(*list_view_);
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kPluginSelectionMessage:
                UpdateDetail();
                return;
            case kPluginRescanMessage:
                Refresh();
                return;
            case kPluginRevealUserRootMessage:
                LaunchPath(shell_host_.UserPluginRootPath());
                return;
            case kPluginRevealAppRootMessage:
                LaunchPath(shell_host_.AppPluginRootPath());
                return;
            case kPluginRevealSelectedMessage:
                RevealSelected();
                return;
            default:
                BGroupView::MessageReceived(message);
                return;
        }
    }

    void Refresh() override {
        std::string rescan_error;
        shell_host_.RescanPlugins(&rescan_error);

        plugins_.clear();
        list_view_->Clear();
        const auto user_root = shell_host_.UserPluginRootPath();
        const auto app_root = shell_host_.AppPluginRootPath();

        std::ostringstream roots;
        roots << "User Plugins: " << user_root.string() << '\n'
              << "App Plugins: " << app_root.string();
        const auto scan_errors = shell_host_.PluginScanErrors();
        if (!scan_errors.empty()) {
            roots << "\n\nLoad Issues:";
            for (const auto& error : scan_errors) {
                roots << "\n- " << error;
            }
        } else if (!rescan_error.empty()) {
            roots << "\n\nStatus: " << rescan_error;
        } else {
            roots << "\n\nStatus: Ready.";
        }
        roots_view_->SetText(roots.str().c_str());

        for (const auto& plugin : shell_host_.Plugins().Plugins()) {
            PluginEntry entry;
            entry.summary = plugin;
            entry.source_root_label = plugin.path.parent_path() == user_root ? "User"
                                      : plugin.path.parent_path() == app_root ? "App"
                                                                              : "Other";
            plugins_.push_back(entry);
        }

        int32 selected_index = -1;
        for (std::size_t index = 0; index < plugins_.size(); ++index) {
            auto* row = new BRow();
            row->SetField(new BStringField(plugins_[index].summary.display_name.c_str()), kPluginFieldName);
            row->SetField(new BStringField(plugins_[index].summary.version.c_str()), kPluginFieldVersion);
            row->SetField(new BStringField(plugins_[index].source_root_label.c_str()), kPluginFieldSourceRoot);
            row->SetField(new BStringField(plugins_[index].summary.path.string().c_str()), kPluginFieldPath);
            row->SetField(new BStringField(std::to_string(plugins_[index].summary.capabilities).c_str()),
                          kPluginFieldCapabilities);
            list_view_->AddRow(row);
            if (!current_identifier_.empty() && plugins_[index].summary.identifier == current_identifier_) {
                selected_index = static_cast<int32>(index);
            }
        }

        if (selected_index < 0 && !plugins_.empty()) {
            selected_index = 0;
        }
        if (selected_index >= 0) {
            if (BRow* row = list_view_->RowAt(selected_index)) {
                list_view_->SetFocusRow(row, true);
            }
        }
        UpdateDetail();
    }

private:
    struct PluginEntry {
        PluginSummary summary;
        std::string source_root_label;
    };

    std::optional<std::size_t> SelectedIndex() const {
        const BRow* selected = list_view_->CurrentSelection();
        const int32 index = selected == nullptr ? -1 : list_view_->IndexOf(selected);
        if (index < 0 || static_cast<std::size_t>(index) >= plugins_.size()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(index);
    }

    void UpdateDetail() {
        const auto index = SelectedIndex();
        if (!index) {
            detail_view_->SetText(plugins_.empty() ? "No plugins are currently available."
                                                   : "Select a plugin to view its details.");
            return;
        }
        const auto& plugin = plugins_[*index];
        current_identifier_ = plugin.summary.identifier;
        std::ostringstream detail;
        detail << "Identifier: " << plugin.summary.identifier << '\n'
               << "Display Name: " << plugin.summary.display_name << '\n'
               << "Version: " << plugin.summary.version << '\n'
               << "Capabilities: " << plugin.summary.capabilities << '\n'
               << "Source Root: " << plugin.source_root_label << '\n'
               << "Path: " << plugin.summary.path.string();
        detail_view_->SetText(detail.str().c_str());
    }

    void RevealSelected() {
        const auto index = SelectedIndex();
        if (!index) {
            return;
        }
        const auto& path = plugins_[*index].summary.path;
        const auto reveal_path = std::filesystem::exists(path) ? path.parent_path() : path.parent_path();
        if (!LaunchPath(reveal_path)) {
            BAlert("plugins", "Unable to reveal the selected plugin.", "OK")->Go();
        }
    }

    BTextView* roots_view_ = nullptr;
    BColumnListView* list_view_ = nullptr;
    BTextView* detail_view_ = nullptr;
    std::vector<PluginEntry> plugins_;
    std::string current_identifier_;
};

class UnsupportedPane final : public WazooPaneBase {
public:
    UnsupportedPane(HaikuShellHost& shell_host, std::string tool_id, std::string title)
        : WazooPaneBase(shell_host, std::move(tool_id)) {
        auto* summary = new BStringView("unsupported-summary", title.c_str());
        auto* detail = new BTextView("unsupported-detail");
        detail->MakeEditable(false);
        detail->SetText("This Wazoo pane is not available in the current shell build.");
        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(summary)
            .Add(new BScrollView("unsupported-scroll", detail, 0, false, true));
    }

    void Refresh() override {}
};

std::unique_ptr<WazooPaneBase> CreatePane(HaikuShellHost& shell_host,
                                          const HaikuWazooWindow::ToolSpec& tool) {
    if (tool.id == "mailboxes") {
        return std::make_unique<MailboxesNavigatorPane>(shell_host);
    }
    if (tool.id == "task-status") {
        return std::make_unique<TaskStatusPane>(shell_host);
    }
    if (tool.id == "task-errors") {
        return std::make_unique<TaskErrorsPane>(shell_host);
    }
    if (tool.id == "signatures") {
        return std::make_unique<SignatureEditorPane>(shell_host);
    }
    if (tool.id == "stationery") {
        return std::make_unique<StationeryEditorPane>(shell_host);
    }
    if (tool.id == "nicknames") {
        return std::make_unique<NicknameManagerPane>(shell_host);
    }
    if (tool.id == "personalities") {
        return std::make_unique<PersonalitiesPane>(shell_host);
    }
    if (tool.id == "filters") {
        return std::make_unique<FilterManagerPane>(shell_host);
    }
    if (tool.id == "filter-report") {
        return std::make_unique<FilterReportTablePane>(shell_host);
    }
    if (tool.id == "link-history") {
        return std::make_unique<LinkHistoryTablePane>(shell_host);
    }
    if (tool.id == "directory-services") {
        return std::make_unique<DirectoryServicesManagerPane>(shell_host);
    }
    if (tool.id == "file-browser") {
        return std::make_unique<FileBrowserManagerPane>(shell_host);
    }
    if (tool.id == "search") {
        return std::make_unique<SearchManagerPane>(shell_host);
    }
    if (tool.id == "plugins") {
        return std::make_unique<PluginManagerPane>(shell_host);
    }
    return std::make_unique<UnsupportedPane>(shell_host, tool.id, tool.title);
}

}  // namespace

HaikuWazooWindow::HaikuWazooWindow(HaikuShellHost& shell_host,
                                   std::string group_id,
                                   std::string title,
                                   std::vector<ToolSpec> tools,
                                   const hermes::WazooWindowState& initial_state)
    : BWindow(BRect(140, 140, 660, 760),
              title.c_str(),
              B_TITLED_WINDOW,
              B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
      shell_host_(shell_host),
      group_id_(std::move(group_id)),
      tools_(std::move(tools)) {
    menu_bar_ = new BMenuBar((group_id_ + "-menu").c_str());
    auto* file_menu = new BMenu("File");
    print_preview_item_ = new BMenuItem("Print Preview", new BMessage(kWazooPrintPreviewMessage));
    print_one_item_ = new BMenuItem("Print One", new BMessage(kWazooPrintDirectMessage));
    file_menu->AddItem(print_preview_item_);
    file_menu->AddItem(print_one_item_);
    menu_bar_->AddItem(file_menu);

    auto* edit_menu = new BMenu("Edit");
    find_item_ = new BMenuItem("Find" B_UTF8_ELLIPSIS, new BMessage(kWazooFindMessage));
    find_again_item_ = new BMenuItem("Find Again", new BMessage(kWazooFindAgainMessage));
    edit_menu->AddItem(find_item_);
    edit_menu->AddItem(find_again_item_);
    menu_bar_->AddItem(edit_menu);

    auto* item_menu = new BMenu("Item");
    new_item_ = new BMenuItem("New", new BMessage(kPaneNewMessage));
    duplicate_item_ = new BMenuItem("Duplicate", new BMessage(kPaneDuplicateGenericMessage));
    save_item_ = new BMenuItem("Save", new BMessage(kPaneSaveMessage));
    delete_item_ = new BMenuItem("Delete", new BMessage(kPaneDeleteMessage));
    move_up_item_ = new BMenuItem("Move Up", new BMessage(kFilterMoveUpMessage));
    move_down_item_ = new BMenuItem("Move Down", new BMessage(kFilterMoveDownMessage));
    compose_item_ = new BMenuItem("New Message To", new BMessage(kPanePrimaryCommandMessage));
    toggle_details_item_ = new BMenuItem("Toggle Details", new BMessage(kPaneSecondaryCommandMessage));
    item_menu->AddItem(new_item_);
    item_menu->AddItem(duplicate_item_);
    item_menu->AddItem(save_item_);
    item_menu->AddItem(delete_item_);
    item_menu->AddSeparatorItem();
    item_menu->AddItem(move_up_item_);
    item_menu->AddItem(move_down_item_);
    item_menu->AddSeparatorItem();
    item_menu->AddItem(compose_item_);
    item_menu->AddItem(toggle_details_item_);
    menu_bar_->AddItem(item_menu);

    tab_view_ = new WazooTabView(this, (group_id_ + "-tabs").c_str());
    auto* content = new BGroupView(B_VERTICAL);
    BLayoutBuilder::Group<>(content, B_VERTICAL, 0).Add(tab_view_);
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu_bar_)
        .Add(content);

    for (const auto& tool : tools_) {
        auto pane = CreatePane(shell_host_, tool);
        if (!pane) {
            continue;
        }
        BView* view = pane.release();
        panes_.push_back(view);
        tab_view_->AddTab(view);
        if (BTab* tab = tab_view_->TabAt(tab_view_->CountTabs() - 1)) {
            tab->SetLabel(tool.title.c_str());
        }
    }

    if (const auto frame = ParseFrame(initial_state.frame)) {
        MoveTo(frame->LeftTop());
        ResizeTo(frame->Width(), frame->Height());
    }
    if (initial_state.selected_tab >= 0 && initial_state.selected_tab < tab_view_->CountTabs()) {
        tab_view_->Select(initial_state.selected_tab);
    }
    AddShortcut('F', B_COMMAND_KEY, new BMessage(kWazooFindMessage));
    AddShortcut(B_F3_KEY, B_NO_COMMAND_KEY, new BMessage(kWazooFindAgainMessage));
    UpdateMenuState();
}

bool HaikuWazooWindow::QuitRequested() {
    for (BView* pane : panes_) {
        if (auto* wazoo_pane = dynamic_cast<WazooPaneBase*>(pane)) {
            if (!wazoo_pane->CanDeactivate()) {
                return false;
            }
            wazoo_pane->PersistState();
        }
    }
    hermes::WazooWindowState state;
    state.open = false;
    state.restore_on_launch = true;
    state.selected_tab = tab_view_ != nullptr ? tab_view_->Selection() : 0;
    state.frame = SerializeFrame(Frame());
    shell_host_.UpdateWazooWindowState(group_id_, state);
    Hide();
    return false;
}

void HaikuWazooWindow::MessageReceived(BMessage* message) {
    const auto active_pane = [this]() -> WazooPaneBase* {
        if (tab_view_ == nullptr) {
            return nullptr;
        }
        const int32 selected_tab = tab_view_->Selection();
        if (selected_tab < 0 || static_cast<std::size_t>(selected_tab) >= panes_.size()) {
            return nullptr;
        }
        return dynamic_cast<WazooPaneBase*>(panes_[static_cast<std::size_t>(selected_tab)]);
    }();
    const std::string active_tool_id = active_pane != nullptr ? active_pane->ToolId() : "";

    switch (message->what) {
        case kWazooPrintPreviewMessage:
            if (active_pane != nullptr && active_pane->CanPrintPreview()) {
                (void)active_pane->HandlePrint(true);
                UpdateMenuState();
            }
            return;

        case kWazooPrintDirectMessage:
            if (active_pane != nullptr && active_pane->CanDirectPrint()) {
                (void)active_pane->HandlePrint(false);
                UpdateMenuState();
            }
            return;

        case kWazooFindMessage:
            if (active_pane != nullptr && active_pane->CanFind()) {
                if (find_window_ == nullptr) {
                    find_window_ = new HaikuFindWindow(
                        BMessenger(this), kWazooFindConfirmedMessage, kWazooFindClosedMessage, "Find");
                }
                auto* find_window = dynamic_cast<HaikuFindWindow*>(find_window_);
                if (find_window != nullptr) {
                    find_window->SetQuery(SharedFindQuery());
                    if (find_window->IsHidden()) {
                        find_window->Show();
                    } else {
                        find_window->Activate(true);
                    }
                    find_window->FocusQuery();
                }
            }
            return;

        case kWazooFindAgainMessage:
            if (active_pane != nullptr && HasSharedFindQuery() && active_pane->CanFindAgain()) {
                active_pane->HandleFind(SharedFindQuery(), true);
                ApplyActivePaneFocus();
            }
            return;

        case kWazooFindConfirmedMessage: {
            const char* query = nullptr;
            if (message->FindString("query", &query) == B_OK && query != nullptr) {
                SetSharedFindQuery(query);
                if (active_pane != nullptr) {
                    active_pane->HandleFind(SharedFindQuery(), false);
                    ApplyActivePaneFocus();
                }
            }
            return;
        }

        case kWazooFindClosedMessage:
            ApplyActivePaneFocus();
            return;

        default:
            if (active_pane != nullptr &&
                active_pane->HandleCommand(RouteWazooCommand(active_tool_id, message->what))) {
                UpdateMenuState();
                return;
            }
            break;
    }

    BWindow::MessageReceived(message);
}

void HaikuWazooWindow::MenusBeginning() {
    BWindow::MenusBeginning();
    UpdateMenuState();
}

void HaikuWazooWindow::WindowActivated(bool active) {
    BWindow::WindowActivated(active);
    if (active) {
        ApplyActivePaneFocus();
    }
}

bool HaikuWazooWindow::HasTool(std::string_view tool_id) const {
    return std::any_of(tools_.begin(), tools_.end(), [&](const auto& tool) { return tool.id == tool_id; });
}

bool HaikuWazooWindow::ActivateTool(std::string_view tool_id) {
    for (std::size_t index = 0; index < tools_.size(); ++index) {
        if (tools_[index].id == tool_id) {
            const int32 previous = tab_view_->Selection();
            tab_view_->Select(static_cast<int32>(index));
            if (tab_view_->Selection() != static_cast<int32>(index) && previous != static_cast<int32>(index)) {
                return false;
            }
            Refresh(tool_id);
            if (IsHidden()) {
                Show();
            } else {
                Activate(true);
            }
            shell_host_.UpdateWazooWindowState(group_id_, CurrentState());
            return true;
        }
    }
    return false;
}

void HaikuWazooWindow::Refresh(std::optional<std::string_view> tool_id) {
    for (std::size_t index = 0; index < panes_.size(); ++index) {
        if (tool_id && tools_[index].id != *tool_id) {
            continue;
        }
        if (auto* pane = dynamic_cast<WazooPaneBase*>(panes_[index])) {
            pane->Refresh();
        }
    }
}

const std::string& HaikuWazooWindow::GroupId() const {
    return group_id_;
}

hermes::WazooWindowState HaikuWazooWindow::CurrentState() const {
    hermes::WazooWindowState state;
    state.open = !IsHidden();
    state.restore_on_launch = true;
    state.selected_tab = tab_view_ != nullptr ? tab_view_->Selection() : 0;
    state.frame = SerializeFrame(Frame());
    return state;
}

bool HaikuWazooWindow::RequestTabSelection(int32 index) {
    if (tab_view_ == nullptr || index == tab_view_->Selection()) {
        return true;
    }
    if (index < 0 || static_cast<std::size_t>(index) >= panes_.size()) {
        return false;
    }

    const int32 current = tab_view_->Selection();
    if (current >= 0 && static_cast<std::size_t>(current) < panes_.size()) {
        if (auto* active_pane = dynamic_cast<WazooPaneBase*>(panes_[static_cast<std::size_t>(current)])) {
            if (!active_pane->CanDeactivate()) {
                return false;
            }
            active_pane->PersistState();
        }
    }
    return true;
}

void HaikuWazooWindow::HandleTabSelectionCommitted() {
    Refresh();
    ApplyActivePaneFocus();
    shell_host_.UpdateWazooWindowState(group_id_, CurrentState());
}

void HaikuWazooWindow::ApplyActivePaneFocus() {
    if (tab_view_ == nullptr) {
        return;
    }
    const int32 current = tab_view_->Selection();
    if (current < 0 || static_cast<std::size_t>(current) >= panes_.size()) {
        return;
    }
    if (auto* active_pane = dynamic_cast<WazooPaneBase*>(panes_[static_cast<std::size_t>(current)])) {
        if (BView* preferred = active_pane->PreferredFocusView()) {
            preferred->MakeFocus(true);
        }
    }
}

void HaikuWazooWindow::UpdateMenuState() {
    const auto active_pane = [this]() -> WazooPaneBase* {
        if (tab_view_ == nullptr) {
            return nullptr;
        }
        const int32 current = tab_view_->Selection();
        if (current < 0 || static_cast<std::size_t>(current) >= panes_.size()) {
            return nullptr;
        }
        return dynamic_cast<WazooPaneBase*>(panes_[static_cast<std::size_t>(current)]);
    }();

    const auto enabled = [active_pane](uint32 command_id) {
        return active_pane != nullptr &&
               active_pane->IsCommandEnabled(RouteWazooCommand(active_pane->ToolId(), command_id));
    };
    const std::string tool_id = active_pane != nullptr ? active_pane->ToolId() : "";

    if (print_preview_item_ != nullptr) {
        print_preview_item_->SetEnabled(active_pane != nullptr && active_pane->CanPrintPreview());
    }
    if (print_one_item_ != nullptr) {
        print_one_item_->SetEnabled(active_pane != nullptr && active_pane->CanDirectPrint());
    }

    if (new_item_ != nullptr) {
        new_item_->SetLabel(tool_id == "mailboxes" ? "New Mailbox"
                           : tool_id == "signatures" ? "New Signature"
                           : tool_id == "stationery" ? "New Stationery"
                                                     : "New");
    }
    if (save_item_ != nullptr) {
        save_item_->SetLabel(tool_id == "mailboxes" ? "Rename" : "Save");
    }
    if (compose_item_ != nullptr) {
        compose_item_->SetLabel(tool_id == "mailboxes" ? "Find Messages" B_UTF8_ELLIPSIS
                               : tool_id == "stationery" ? "New Message With"
                               : tool_id == "link-history" ? "Open"
                                                           : "New Message To");
    }
    if (toggle_details_item_ != nullptr) {
        toggle_details_item_->SetLabel(tool_id == "mailboxes" ? "Refresh"
                                      : tool_id == "signatures" ? "Reveal on Disk"
                                      : tool_id == "stationery" ? "Reply With"
                                      : tool_id == "link-history" ? "Clear"
                                                                  : "Toggle Details");
    }

    if (find_item_ != nullptr) {
        find_item_->SetEnabled(active_pane != nullptr && active_pane->CanFind());
    }
    if (find_again_item_ != nullptr) {
        find_again_item_->SetEnabled(active_pane != nullptr && HasSharedFindQuery() &&
                                     active_pane->CanFindAgain());
    }
    if (new_item_ != nullptr) {
        new_item_->SetEnabled(enabled(kPaneNewMessage));
    }
    if (duplicate_item_ != nullptr) {
        duplicate_item_->SetEnabled(enabled(kPaneDuplicateGenericMessage));
    }
    if (save_item_ != nullptr) {
        save_item_->SetEnabled(enabled(kPaneSaveMessage));
    }
    if (delete_item_ != nullptr) {
        delete_item_->SetEnabled(enabled(kPaneDeleteMessage));
    }
    if (move_up_item_ != nullptr) {
        move_up_item_->SetEnabled(enabled(kFilterMoveUpMessage));
    }
    if (move_down_item_ != nullptr) {
        move_down_item_->SetEnabled(enabled(kFilterMoveDownMessage));
    }
    if (compose_item_ != nullptr) {
        compose_item_->SetEnabled(enabled(kPanePrimaryCommandMessage));
    }
    if (toggle_details_item_ != nullptr) {
        toggle_details_item_->SetEnabled(enabled(kPaneSecondaryCommandMessage));
    }
}

}  // namespace hemera::haiku
