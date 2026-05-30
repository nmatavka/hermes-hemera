#include "HaikuMainWindow.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <Control.h>
#include <Entry.h>
#include <Font.h>
#include <GroupView.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Message.h>
#include <MessageFilter.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <ScrollView.h>
#include <Size.h>
#include <SplitView.h>
#include <StringItem.h>
#include <StringView.h>
#include <Tab.h>
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>
#include <View.h>
#include <private/interface/ColumnListView.h>
#include <private/interface/ColumnTypes.h>

#include "HaikuFindSupport.h"
#include "HaikuShellHost.h"
#include "HaikuToolbarSupport.h"
#include "HaikuWebKitSupport.h"
#include "hermes/ComposeMessage.h"
#include "hermes/HemeraIdentity.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageRenderer.h"
#include "hermes/MessageStore.h"
#include "hermes/NicknameStore.h"
#include "hermes/RichTextFormat.h"
#include "hermes/SearchBarSettings.h"
#include "hermes/SelectedTextUrlSettings.h"
#include "hermes/ToolbarConfiguration.h"
#include "hermes/WorkspaceModel.h"

namespace hemera::haiku {

namespace {

constexpr uint32_t kNewComposeMessage = 'ncmp';
constexpr uint32_t kMessageSelectedMessage = 'mmsg';
constexpr uint32_t kAttachmentSelectedMessage = 'atts';
constexpr uint32_t kSendQueuedMessage = 'sndq';
constexpr uint32_t kCheckMailMessage = 'ckml';
constexpr uint32_t kSendReceiveMessage = 'sdrx';
constexpr uint32_t kStopTasksMessage = 'stpt';
constexpr uint32_t kRefreshMailboxMessage = 'mbrf';
constexpr uint32_t kResyncMailboxMessage = 'mbrs';
constexpr uint32_t kCreateMailboxMessage = 'mbcr';
constexpr uint32_t kRenameMailboxMessage = 'mbrn';
constexpr uint32_t kDeleteMailboxMessage = 'mbdl';
constexpr uint32_t kCreateMailboxConfirmed = 'mbcc';
constexpr uint32_t kRenameMailboxConfirmed = 'mbrc';
constexpr uint32_t kDeleteMessageMessage = 'msgd';
constexpr uint32_t kOpenMessageMessage = 'msgo';
constexpr uint32_t kUndeleteMessageMessage = 'msgu';
constexpr uint32_t kPurgeMailboxMessage = 'mpge';
constexpr uint32_t kReplyMessage = 'rply';
constexpr uint32_t kReplyAllMessage = 'rpal';
constexpr uint32_t kForwardMessage = 'frwd';
constexpr uint32_t kRedirectMessage = 'rdrt';
constexpr uint32_t kSendAgainMessage = 'sagn';
constexpr uint32_t kSendImmediatelyMessage = 'simm';
constexpr uint32_t kChangeQueueingMessage = 'chqu';
constexpr uint32_t kChangeQueueingConfirmedMessage = 'cqok';
constexpr uint32_t kCtrlFMessage = 'mcfn';
constexpr uint32_t kCtrlShiftFMessage = 'mcsf';
constexpr uint32_t kFindTextMessage = 'mfin';
constexpr uint32_t kFindAgainMessage = 'mfag';
constexpr uint32_t kFindConfirmedMessage = 'mffc';
constexpr uint32_t kFindClosedMessage = 'mflc';
constexpr uint32_t kFindMessagesMessage = 'fdmg';
constexpr uint32_t kCtrlJShortcutMessage = 'mcjj';
constexpr uint32_t kCtrlShiftJShortcutMessage = 'mcsj';
constexpr uint32_t kCtrlShiftLShortcutMessage = 'mcsl';
constexpr uint32_t kMarkReadMessage = 'msgr';
constexpr uint32_t kMarkUnreadMessage = 'mkun';
constexpr uint32_t kSetLegacyStatusMessage = 'msts';
constexpr uint32_t kSetLabelMessage = 'mlbl';
constexpr uint32_t kServerLeaveMessage = 'pslv';
constexpr uint32_t kServerFetchMessage = 'psft';
constexpr uint32_t kServerDeleteMessage = 'psdl';
constexpr uint32_t kServerFetchDeleteMessage = 'psfd';
constexpr uint32_t kMarkJunkMessage = 'mjnk';
constexpr uint32_t kMarkNotJunkMessage = 'mnoj';
constexpr uint32_t kRecheckJunkMessage = 'mrjk';
constexpr uint32_t kFilterMessagesMessage = 'mflt';
constexpr uint32_t kMakeFilterMessage = 'mkfl';
constexpr uint32_t kPriorityHighestMessage = 'prh1';
constexpr uint32_t kPriorityHighMessage = 'prh2';
constexpr uint32_t kPriorityNormalMessage = 'prn3';
constexpr uint32_t kPriorityLowMessage = 'prl4';
constexpr uint32_t kPriorityLowestMessage = 'prl5';
constexpr uint32_t kMakeNicknameMessage = 'mknk';
constexpr uint32_t kPrintPreviewMessage = 'prpv';
constexpr uint32_t kPrintDirectMessage = 'prdr';
constexpr uint32_t kMoveMessageMessage = 'msgm';
constexpr uint32_t kCopyMessageMessage = 'msgc';
constexpr uint32_t kPerformMoveMessage = 'pmov';
constexpr uint32_t kPerformCopyMessage = 'pcpy';
constexpr uint32_t kFetchFullMessageMessage = 'msgf';
constexpr uint32_t kFetchDefaultMessageMessage = 'msdf';
constexpr uint32_t kImapRedownloadFullMessage = 'irdf';
constexpr uint32_t kImapRedownloadDefaultMessage = 'irdd';
constexpr uint32_t kImapClearCachedMessage = 'icch';
constexpr uint32_t kOpenAttachmentMessage = 'atop';
constexpr uint32_t kSaveAttachmentMessage = 'atsv';
constexpr uint32_t kSaveAllAttachmentsMessage = 'atsa';
constexpr uint32_t kFetchAttachmentMessage = 'atfe';
constexpr uint32_t kRetryTaskMessage = 'trty';
constexpr uint32_t kCancelTaskMessage = 'tcnl';
constexpr uint32_t kPromptAcceptedMessage = 'prok';
constexpr uint32_t kPromptCancelledMessage = 'prcl';
constexpr uint32_t kTogglePreviewPaneMessage = 'tgpp';
constexpr uint32_t kToggleToolbarMessage = 'tgtb';
constexpr uint32_t kToggleSearchBarMessage = 'tgsb';
constexpr uint32_t kToggleUtilityPaneMessage = 'tgup';
constexpr uint32_t kSelectTaskStatusTabMessage = 'stst';
constexpr uint32_t kSelectTaskErrorsTabMessage = 'ster';
constexpr uint32_t kOpenToolWindowMessage = 'otwl';
constexpr uint32_t kCustomizeToolbarMessage = 'cttb';
constexpr uint32_t kPreviewReadTickMessage = 'prrd';
constexpr uint32_t kPreviewRefreshTickMessage = 'prrf';
constexpr uint32_t kAutoFetchPreviewMessage = 'prff';
constexpr uint32_t kImportFromEudoraMessage = 'impe';
constexpr uint32_t kOpenHelpContentsMessage = 'hlpc';
constexpr uint32_t kRevealHelpFilesMessage = 'hlpr';
constexpr uint32_t kSpecialWorkOfflineMessage = 'spof';
constexpr uint32_t kSpecialEmptyTrashMessage = 'spet';
constexpr uint32_t kSpecialTrimJunkMessage = 'sptj';
constexpr uint32_t kSpecialCompactMailboxesMessage = 'spcm';
constexpr uint32_t kSpecialForgetPasswordsMessage = 'spfp';
constexpr uint32_t kSpecialChangePasswordMessage = 'spcp';
constexpr uint32_t kSpecialOptionsMessage = 'spop';
constexpr uint32_t kPreviousMessageMessage = 'mprv';
constexpr uint32_t kNextMessageMessage = 'mnxt';
constexpr uint32_t kToggleStatusMessage = 'mtgl';
constexpr uint32_t kOpenTaskStatusWindowMessage = 'wtsk';
constexpr uint32_t kOpenTaskErrorsWindowMessage = 'wter';
constexpr uint32_t kDynamicNewStationeryMessage = 'dyns';
constexpr uint32_t kDynamicReplyStationeryMessage = 'dyrs';
constexpr uint32_t kDynamicReplyAllStationeryMessage = 'dyra';
constexpr uint32_t kDynamicChangePersonaMessage = 'dycp';
constexpr uint32_t kDynamicNewRecipientMessage = 'dynr';
constexpr uint32_t kDynamicForwardRecipientMessage = 'dyfr';
constexpr uint32_t kDynamicRedirectRecipientMessage = 'dyrd';
constexpr uint32_t kOpenRecentMailboxMessage = 'ormb';
constexpr uint32_t kSelectedTextUrl1Message = 'stu1';
constexpr uint32_t kSelectedTextUrl2Message = 'stu2';
constexpr uint32_t kSelectedTextUrl3Message = 'stu3';
constexpr uint32_t kSelectedTextUrl4Message = 'stu4';
constexpr uint32_t kSelectedTextUrl5Message = 'stu5';
constexpr uint32_t kSelectedTextUrl6Message = 'stu6';
constexpr uint32_t kSelectedTextUrl7Message = 'stu7';
constexpr uint32_t kWindowSendBehindMessage = 'wmsb';
constexpr uint32_t kWindowCascadeMessage = 'wmcs';
constexpr uint32_t kWindowTileHorizontalMessage = 'wmth';
constexpr uint32_t kWindowTileVerticalMessage = 'wmtv';
constexpr uint32_t kWindowArrangeMessage = 'wmar';
constexpr uint32_t kWindowCloseAllMessage = 'wmca';
constexpr uint32_t kPasswordChangedMessage = 'pwok';
constexpr uint32_t kOptionsAcceptedMessage = 'opok';
constexpr uint32_t kMailTransferOptionsAcceptedMessage = 'mtok';
constexpr uint32_t kBackspaceDeleteMessage = 'bkdl';
constexpr uint32_t kTocInvokeMessage = 'tciv';
constexpr uint32_t kGroupBySubjectMessage = 'grsb';
constexpr uint32_t kReplyPrimaryShortcutMessage = 'rrp1';
constexpr uint32_t kReplySecondaryShortcutMessage = 'rrp2';
constexpr uint32_t kSearchBarExecuteMessage = 'srex';
constexpr uint32_t kSearchBarScopeChangedMessage = 'srsc';
constexpr uint32_t kSearchBarQueryModifiedMessage = 'srqm';
constexpr uint32_t kSearchBarActionChosenMessage = 'srac';
constexpr uint32_t kSearchBarRecentChosenMessage = 'srrc';
constexpr uint32_t kSearchBarFocusChangedMessage = 'srfc';

constexpr float kHeaderInset = 8.0f;
constexpr float kMessageItemMinHeight = 38.0f;
constexpr float kTocHeaderActivationHeight = 24.0f;
constexpr int kDefaultMultiReplyWarnThreshold = 50;
constexpr int kSelectedTextUrlSlotCount = 7;

enum TocFieldIndex : int32_t {
    kTocFieldStatus = 0,
    kTocFieldJunk,
    kTocFieldLabel,
    kTocFieldPriority,
    kTocFieldAttachment,
    kTocFieldFrom,
    kTocFieldDate,
    kTocFieldSize,
    kTocFieldPopServerStatus,
    kTocFieldDownload,
    kTocFieldSubject,
};

int MaxRecentMailboxCount(const IniSettingsStore& settings) {
    return std::clamp(settings.GetInt("Settings", "MaxRecentMailbox", 10), 0, 99);
}

ComposeMessage BuildDefaultComposeMessage(HaikuShellHost& shell_host) {
    ComposeMessage message;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    message.id = "compose-" + std::to_string(static_cast<long long>(micros));
    message.policy = ComposePolicyFromSettings(shell_host.Settings());
    message.signature_name = message.policy.default_signature_name;
    message.stationery_name = message.policy.default_stationery_name;
    return message;
}

std::filesystem::path DefaultAttachmentSaveRoot() {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Downloads" /
               std::string(hermes::kHemeraDownloadsDirectoryName);
    }
    return std::filesystem::temp_directory_path() / std::string(hermes::kHemeraDownloadsDirectoryName);
}

std::string JoinLines(const std::vector<std::string>& lines) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index != 0) {
            stream << '\n';
        }
        stream << lines[index];
    }
    return stream.str();
}

std::string JoinBulletList(const std::vector<std::string>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            stream << "  •  ";
        }
        stream << values[index];
    }
    return stream.str();
}

std::string JoinCommaList(const std::vector<std::string>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            stream << ", ";
        }
        stream << values[index];
    }
    return stream.str();
}

const char* ReplyMenuLabel(bool reply_all) {
    return reply_all ? "Reply All" : "Reply";
}

const char* SendImmediateMenuLabel(bool immediate_send) {
    return immediate_send ? "Send Immediately" : "Queue For Delivery";
}

uint32_t SelectedTextUrlCommandForSlot(int slot) {
    switch (slot) {
        case 1:
            return kSelectedTextUrl1Message;
        case 2:
            return kSelectedTextUrl2Message;
        case 3:
            return kSelectedTextUrl3Message;
        case 4:
            return kSelectedTextUrl4Message;
        case 5:
            return kSelectedTextUrl5Message;
        case 6:
            return kSelectedTextUrl6Message;
        case 7:
            return kSelectedTextUrl7Message;
        default:
            return 0;
    }
}

int SelectedTextUrlSlotForCommand(uint32_t command) {
    switch (command) {
        case kSelectedTextUrl1Message:
            return 1;
        case kSelectedTextUrl2Message:
            return 2;
        case kSelectedTextUrl3Message:
            return 3;
        case kSelectedTextUrl4Message:
            return 4;
        case kSelectedTextUrl5Message:
            return 5;
        case kSelectedTextUrl6Message:
            return 6;
        case kSelectedTextUrl7Message:
            return 7;
        default:
            return 0;
    }
}

class SearchBarTextFilter final : public BMessageFilter {
public:
    explicit SearchBarTextFilter(BWindow* owner)
        : BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE),
          owner_(owner) {}

    filter_result Filter(BMessage* message, BHandler** target) override {
        (void)target;
        if (owner_ == nullptr || message == nullptr) {
            return B_DISPATCH_MESSAGE;
        }

        if (message->what == B_FOCUS_CHANGED) {
            bool focused = false;
            if (message->FindBool("be:focus", &focused) == B_OK) {
                BMessage focus_message(kSearchBarFocusChangedMessage);
                focus_message.AddBool("focused", focused);
                BMessenger(owner_).SendMessage(&focus_message);
            }
            return B_DISPATCH_MESSAGE;
        }

        if (message->what != B_KEY_DOWN) {
            return B_DISPATCH_MESSAGE;
        }
        const char* bytes = nullptr;
        if (message->FindString("bytes", &bytes) != B_OK || bytes == nullptr) {
            return B_DISPATCH_MESSAGE;
        }
        if (bytes[0] == B_ENTER || bytes[0] == '\n') {
            BMessenger(owner_).SendMessage(kSearchBarExecuteMessage);
            return B_SKIP_MESSAGE;
        }
        return B_DISPATCH_MESSAGE;
    }

private:
    BWindow* owner_ = nullptr;
};

std::string EscapeForOpenCommand(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        if (ch == '"' || ch == '\\' || ch == '$' || ch == '`') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

bool LaunchExternalTarget(std::string_view target) {
    if (target.empty()) {
        return false;
    }
    const std::string command = "open \"" + EscapeForOpenCommand(target) + "\" >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}

std::string SelectedTextFromView(const BTextView* text_view) {
    if (text_view == nullptr || text_view->Text() == nullptr) {
        return {};
    }
    int32 start = 0;
    int32 end = 0;
    text_view->GetSelection(&start, &end);
    if (end <= start) {
        return {};
    }
    return TrimWhitespace(std::string(text_view->Text() + start, static_cast<std::size_t>(end - start)));
}

std::string SummarizeTransportResult(const MailTransportSummary& summary,
                                     std::string_view success_message,
                                     std::string_view fallback_message) {
    if (summary.success) {
        return std::string(success_message);
    }
    if (!summary.error_message.empty()) {
        return summary.error_message;
    }
    if (!summary.warnings.empty()) {
        return std::string(fallback_message) + ": " + summary.warnings.front();
    }
    return std::string(fallback_message);
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string FormatJunkScoreField(const MessageSummary& summary) {
    if (summary.junk_score <= 0 && !summary.manually_junked) {
        return {};
    }
    std::ostringstream stream;
    stream << std::setw(3) << std::setfill('0') << std::clamp(summary.junk_score, 0, 127);
    return stream.str();
}

std::string FormatStatusField(const MessageSummary& summary) {
    return LegacyMessageStatusLabel(summary.legacy_status);
}

std::string FormatLabelField(const MessageSummary& summary, const MailboxUiSettings& mailbox_ui) {
    return summary.label_index > 0 ? MailboxLabelName(mailbox_ui, summary.label_index) : std::string{};
}

std::string FormatPopServerStatusField(const MessageSummary& summary) {
    return PopServerStatusLabel(summary.pop_server_status);
}

int PrioritySortRank(std::string_view priority) {
    const std::string lowered = ToLower(std::string(priority));
    if (lowered == "highest") {
        return 0;
    }
    if (lowered == "high") {
        return 1;
    }
    if (lowered == "normal") {
        return 2;
    }
    if (lowered == "low") {
        return 3;
    }
    if (lowered == "lowest") {
        return 4;
    }
    return 2;
}

bool IsLikelyOutgoingSummary(const MessageSummary& summary, std::string_view mailbox_id) {
    switch (summary.legacy_status) {
        case LegacyMessageStatus::kSendable:
        case LegacyMessageStatus::kQueued:
        case LegacyMessageStatus::kTimeQueued:
        case LegacyMessageStatus::kSent:
        case LegacyMessageStatus::kUnsent:
            return true;
        default:
            break;
    }
    const std::string lowered_status = ToLower(summary.status);
    if (lowered_status == "queued" || lowered_status == "sending" || lowered_status == "sent" ||
        lowered_status == "draft") {
        return true;
    }
    const std::string lowered_mailbox = ToLower(std::string(mailbox_id));
    return lowered_mailbox == "out" || lowered_mailbox == "sent" || lowered_mailbox == "drafts";
}

TocRowStyle BuildTocRowStyle(const MessageSummary& summary,
                             const MailboxUiSettings& mailbox_ui,
                             std::string_view mailbox_id) {
    TocRowStyle style;
    style.italic = mailbox_ui.comp_summary_italic && IsLikelyOutgoingSummary(summary, mailbox_id);
    style.draw_bottom_line = mailbox_ui.show_mailbox_lines;
    style.line_color = mailbox_ui.black_toc_lines ? rgb_color{0, 0, 0, 255} : rgb_color{184, 184, 184, 255};
    if (mailbox_ui.whole_summary_label_color && summary.label_index > 0) {
        if (const auto label = FindMailboxLabelDefinition(mailbox_ui, summary.label_index)) {
            style.use_label_color = true;
            style.label_color = rgb_color{
                static_cast<uint8>(std::clamp(label->red, 0, 255)),
                static_cast<uint8>(std::clamp(label->green, 0, 255)),
                static_cast<uint8>(std::clamp(label->blue, 0, 255)),
                255};
        }
    }
    return style;
}

int CompareTocField(const MessageSummary& left, const MessageSummary& right, int32 field_index) {
    switch (field_index) {
        case kTocFieldStatus: {
            const int left_rank = LegacyMessageStatusSortRank(left.legacy_status);
            const int right_rank = LegacyMessageStatusSortRank(right.legacy_status);
            return left_rank < right_rank ? -1 : (left_rank > right_rank ? 1 : 0);
        }
        case kTocFieldJunk:
            return left.junk_score < right.junk_score ? -1 : (left.junk_score > right.junk_score ? 1 : 0);
        case kTocFieldLabel:
            return left.label_index < right.label_index ? -1 : (left.label_index > right.label_index ? 1 : 0);
        case kTocFieldPriority: {
            const int left_rank = PrioritySortRank(left.priority);
            const int right_rank = PrioritySortRank(right.priority);
            return left_rank < right_rank ? -1 : (left_rank > right_rank ? 1 : 0);
        }
        case kTocFieldAttachment:
            return left.attachment_count < right.attachment_count ? -1
                                                                  : (left.attachment_count > right.attachment_count ? 1 : 0);
        case kTocFieldFrom:
            return ToLower(left.sender).compare(ToLower(right.sender));
        case kTocFieldDate:
            return left.timestamp < right.timestamp ? -1 : (left.timestamp > right.timestamp ? 1 : 0);
        case kTocFieldSize:
            return left.size < right.size ? -1 : (left.size > right.size ? 1 : 0);
        case kTocFieldPopServerStatus: {
            const int left_rank = PopServerStatusSortRank(left.pop_server_status);
            const int right_rank = PopServerStatusSortRank(right.pop_server_status);
            return left_rank < right_rank ? -1 : (left_rank > right_rank ? 1 : 0);
        }
        case kTocFieldDownload: {
            const int left_rank = (left.download_complete && !left.attachments_omitted) ? 1 : 0;
            const int right_rank = (right.download_complete && !right.attachments_omitted) ? 1 : 0;
            return left_rank < right_rank ? -1 : (left_rank > right_rank ? 1 : 0);
        }
        case kTocFieldSubject:
            return CompareMailboxSubjects(left.subject, right.subject);
        default:
            return 0;
    }
}

int ApplyDirection(int comparison, bool descending) {
    if (comparison == 0) {
        return 0;
    }
    return descending ? -comparison : comparison;
}

std::string TrimWhitespace(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::string ActionKindLabel(ImapActionKind kind) {
    switch (kind) {
        case ImapActionKind::kDelete:
            return "Delete";
        case ImapActionKind::kUndelete:
            return "Undelete";
        case ImapActionKind::kExpungeMailbox:
            return "Purge/Expunge";
        case ImapActionKind::kMove:
            return "Move";
        case ImapActionKind::kCopy:
            return "Copy";
        case ImapActionKind::kCreateMailbox:
            return "Create Mailbox";
        case ImapActionKind::kRenameMailbox:
            return "Rename Mailbox";
        case ImapActionKind::kDeleteMailbox:
            return "Delete Mailbox";
        case ImapActionKind::kFetchAttachment:
            return "Fetch Attachment";
        case ImapActionKind::kFetchDefaultMessage:
            return "Fetch Default Message";
        case ImapActionKind::kFetchFullMessage:
            return "Fetch Full Message";
        case ImapActionKind::kRedownloadDefaultMessage:
            return "Redownload Default Message";
        case ImapActionKind::kRedownloadFullMessage:
            return "Redownload Full Message";
        case ImapActionKind::kResyncMailbox:
            return "Resync Mailbox";
        case ImapActionKind::kRefreshMailboxList:
            return "Refresh Mailboxes";
    }
    return "IMAP Action";
}

std::string ActionStateLabel(ImapActionState state) {
    switch (state) {
        case ImapActionState::kPending:
            return "Pending";
        case ImapActionState::kRunning:
            return "Running";
        case ImapActionState::kFailed:
            return "Failed";
        case ImapActionState::kCompleted:
            return "Completed";
        case ImapActionState::kCancelled:
            return "Cancelled";
    }
    return "Pending";
}

std::string AttachmentLabel(const AttachmentSummary& attachment) {
    std::ostringstream label;
    label << (attachment.name.empty() ? "(unnamed attachment)" : attachment.name);
    if (attachment.size > 0) {
        label << " (" << attachment.size << " bytes)";
    }
    if (attachment.omitted || !attachment.download_complete) {
        label << " [fetch required]";
    }
    if (!attachment.fetch_error.empty()) {
        label << " [error: " << attachment.fetch_error << "]";
    }
    return label.str();
}

std::string FormatTimestamp(std::int64_t value) {
    if (value <= 0) {
        return "";
    }

    const std::time_t timestamp = static_cast<std::time_t>(value);
    const std::tm* local_time = std::localtime(&timestamp);
    if (local_time == nullptr) {
        return "";
    }

    char buffer[64];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", local_time) == 0) {
        return "";
    }
    return buffer;
}

std::string FormatMessageSize(std::size_t size) {
    if (size >= 1024 * 1024) {
        return std::to_string(static_cast<int>(size / (1024 * 1024))) + " MB";
    }
    if (size >= 1024) {
        return std::to_string(static_cast<int>(size / 1024)) + " KB";
    }
    return std::to_string(size) + " B";
}

bool LaunchPath(const std::filesystem::path& path) {
    if (be_roster == nullptr) {
        return false;
    }
    BEntry entry(path.c_str(), true);
    if (entry.InitCheck() != B_OK) {
        return false;
    }
    entry_ref ref;
    if (entry.GetRef(&ref) != B_OK) {
        return false;
    }
    return be_roster->Launch(&ref) == B_OK;
}

void ShowInfoAlert(const char* title, const std::string& message) {
    BAlert(title, message.c_str(), "OK")->Go();
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

void UpdateCommandControls(BView* view, const std::function<bool(uint32)>& enabled) {
    if (view == nullptr) {
        return;
    }
    if (auto* control = dynamic_cast<BControl*>(view)) {
        if (BMessage* message = control->Message()) {
            control->SetEnabled(enabled(message->what));
        }
    }
    for (int32 index = 0; BView* child = view->ChildAt(index); ++index) {
        UpdateCommandControls(child, enabled);
    }
}

class ContextListView final : public BListView {
public:
    using ContextHandler = std::function<void(BPoint)>;

    ContextListView(const char* name, BMessage* selection_message, ContextHandler handler)
        : BListView(name),
          handler_(std::move(handler)) {
        if (selection_message != nullptr) {
            SetSelectionMessage(selection_message);
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

private:
    ContextHandler handler_;
};

class ContextTextView final : public BTextView {
public:
    using ContextHandler = std::function<void(BPoint)>;

    ContextTextView(const char* name, ContextHandler handler)
        : BTextView(name),
          handler_(std::move(handler)) {}

    void MouseDown(BPoint where) override {
        uint32 buttons = 0;
        GetMouse(&where, &buttons, false);
        if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
            MakeFocus(true);
            if (handler_) {
                handler_(where);
            }
            return;
        }
        BTextView::MouseDown(where);
    }

private:
    ContextHandler handler_;
};

class ContextColumnListView final : public BColumnListView {
public:
    using ContextHandler = std::function<void(BPoint)>;
    using HeaderClickHandler = std::function<void(BPoint, uint32)>;

    ContextColumnListView(const char* name,
                          BMessage* selection_message,
                          BMessage* invocation_message,
                          ContextHandler handler,
                          HeaderClickHandler header_click_handler = {})
        : BColumnListView(name, B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE),
          handler_(std::move(handler)),
          header_click_handler_(std::move(header_click_handler)) {
        SetSelectionMode(B_MULTIPLE_SELECTION_LIST);
        SetSelectionMessage(selection_message);
        SetInvocationMessage(invocation_message);
        SetSortingEnabled(false);
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
        header_sort_tracking_ = false;
        if ((buttons & B_PRIMARY_MOUSE_BUTTON) != 0 && header_click_handler_ != nullptr &&
            where.y <= kTocHeaderActivationHeight && ColumnAt(where) != nullptr && RowAt(where) == nullptr) {
            header_sort_tracking_ = true;
            header_sort_where_ = where;
            header_sort_modifiers_ = modifiers();
        }
        BColumnListView::MouseDown(where);
    }

    void MouseUp(BPoint where) override {
        const bool should_sort = header_sort_tracking_ &&
                                 std::fabs(where.x - header_sort_where_.x) <= 4.0f &&
                                 std::fabs(where.y - header_sort_where_.y) <= 4.0f &&
                                 where.y <= kTocHeaderActivationHeight && ColumnAt(where) != nullptr &&
                                 RowAt(where) == nullptr;
        BColumnListView::MouseUp(where);
        if (should_sort && header_click_handler_ != nullptr) {
            header_click_handler_(where, header_sort_modifiers_);
        }
        header_sort_tracking_ = false;
    }

private:
    ContextHandler handler_;
    HeaderClickHandler header_click_handler_;
    bool header_sort_tracking_ = false;
    BPoint header_sort_where_{};
    uint32 header_sort_modifiers_ = 0;
};

struct TocRowStyle {
    bool italic = false;
    bool use_label_color = false;
    rgb_color label_color{0, 0, 0, 255};
    bool draw_bottom_line = false;
    rgb_color line_color{184, 184, 184, 255};
};

class TocMessageRow;

class TocStringField final : public BStringField {
public:
    TocStringField(const char* string, const TocMessageRow* row, TocRowStyle style)
        : BStringField(string),
          row_(row),
          style_(style) {}

    const TocMessageRow* Row() const { return row_; }
    const TocRowStyle& Style() const { return style_; }

private:
    const TocMessageRow* row_ = nullptr;
    TocRowStyle style_{};
};

class TocStyledColumn : public BStringColumn {
public:
    TocStyledColumn(const char* title,
                    int32 field_index,
                    float width,
                    float min_width,
                    float max_width,
                    uint32 truncate,
                    alignment align = B_ALIGN_LEFT)
        : BStringColumn(title, width, min_width, max_width, truncate, align),
          field_index_(field_index) {}

    void DrawField(BField* field, BRect rect, BView* parent) override;
    int CompareFields(BField* field1, BField* field2) override;

private:
    int CompareRows(const TocMessageRow& left, const TocMessageRow& right) const;

    int32 field_index_ = 0;
};

class TextPromptWindow final : public BWindow {
public:
    TextPromptWindow(const char* title,
                     const char* label,
                     const std::string& initial_value,
                     const BMessenger& target,
                     BMessage payload)
        : BWindow(BRect(0, 0, 360, 120),
                  title,
                  B_TITLED_WINDOW,
                  B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
          target_(target),
          payload_(std::move(payload)) {
        input_ = new BTextControl("prompt-input", label, initial_value.c_str(), nullptr);

        auto* cancel = new BButton("cancel-button", "Cancel", new BMessage(kPromptCancelledMessage));
        auto* ok = new BButton("ok-button", "OK", new BMessage(kPromptAcceptedMessage));
        SetDefaultButton(ok);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(input_)
            .AddGroup(B_HORIZONTAL, 8)
                .AddGlue()
                .Add(cancel)
                .Add(ok)
            .End();
    }

    void MessageReceived(BMessage* message) override {
        switch (message->what) {
            case kPromptAcceptedMessage: {
                BMessage response(payload_);
                response.AddString("name", input_->Text());
                target_.SendMessage(&response);
                PostMessage(B_QUIT_REQUESTED);
                return;
            }

            case kPromptCancelledMessage:
                PostMessage(B_QUIT_REQUESTED);
                return;

            default:
                BWindow::MessageReceived(message);
                return;
        }
    }

private:
    BMessenger target_;
    BMessage payload_;
    BTextControl* input_ = nullptr;
};

class PasswordPromptWindow final : public BWindow {
public:
    PasswordPromptWindow(const std::vector<AccountProfile>& accounts, const BMessenger& target)
        : BWindow(BRect(0, 0, 460, 190),
                  "Change Password",
                  B_TITLED_WINDOW,
                  B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
          target_(target) {
        auto* account_menu = new BPopUpMenu("password-account-menu");
        for (const auto& account : accounts) {
            auto* payload = new BMessage('acct');
            auto* item =
                new BMenuItem(account.display_name.empty() ? account.id.c_str() : account.display_name.c_str(),
                              payload);
            item->Message()->AddString("account_id", account.id.c_str());
            account_menu->AddItem(item);
        }
        if (account_menu->ItemAt(0) != nullptr) {
            account_menu->ItemAt(0)->SetMarked(true);
        }
        account_field_ = new BMenuField("password-account-field", "Account:", account_menu);
        incoming_password_ = new BTextControl("incoming-password", "Incoming:", "", nullptr);
        outgoing_password_ = new BTextControl("outgoing-password", "Outgoing:", "", nullptr);
        if (incoming_password_->TextView() != nullptr) {
            incoming_password_->TextView()->HideTyping(true);
        }
        if (outgoing_password_->TextView() != nullptr) {
            outgoing_password_->TextView()->HideTyping(true);
        }

        auto* cancel = new BButton("password-cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
        auto* ok = new BButton("password-ok", "Apply", new BMessage(kPasswordChangedMessage));
        SetDefaultButton(ok);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(account_field_)
            .Add(incoming_password_)
            .Add(outgoing_password_)
            .AddGroup(B_HORIZONTAL, 8)
                .AddGlue()
                .Add(cancel)
                .Add(ok)
            .End();
    }

    void MessageReceived(BMessage* message) override {
        if (message->what == kPasswordChangedMessage) {
            BMessage payload(kPasswordChangedMessage);
            if (const auto* marked = account_field_->Menu()->FindMarked(); marked != nullptr &&
                marked->Message() != nullptr) {
                const char* account_id = nullptr;
                if (marked->Message()->FindString("account_id", &account_id) == B_OK && account_id != nullptr) {
                    payload.AddString("account_id", account_id);
                }
            }
            payload.AddString("incoming_password", incoming_password_->Text());
            payload.AddString("outgoing_password", outgoing_password_->Text());
            target_.SendMessage(&payload);
            PostMessage(B_QUIT_REQUESTED);
            return;
        }
        BWindow::MessageReceived(message);
    }

private:
    BMessenger target_;
    BMenuField* account_field_ = nullptr;
    BTextControl* incoming_password_ = nullptr;
    BTextControl* outgoing_password_ = nullptr;
};

class MailTransferOptionsWindow final : public BWindow {
public:
    MailTransferOptionsWindow(const std::vector<AccountProfile>& accounts,
                              const MailTransferSettings& settings,
                              bool sending_only,
                              const BMessenger& target)
        : BWindow(BRect(0, 0, 520, 420),
                  sending_only ? "Send Queued Mail" : "Mail Transfer Options",
                  B_TITLED_WINDOW,
                  B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
          account_ids_(),
          target_(target),
          sending_only_(sending_only) {
        send_box_ =
            new BCheckBox("transfer-send", "Send queued messages", nullptr);
        retrieve_new_box_ =
            new BCheckBox("transfer-retrieve-new", "Retrieve new messages", nullptr);
        delete_marked_box_ =
            new BCheckBox("transfer-delete-marked", "Delete marked server messages", nullptr);
        retrieve_marked_box_ =
            new BCheckBox("transfer-retrieve-marked", "Retrieve marked server messages", nullptr);
        delete_retrieved_box_ =
            new BCheckBox("transfer-delete-retrieved", "Delete retrieved server messages", nullptr);
        delete_all_box_ =
            new BCheckBox("transfer-delete-all", "Delete all server messages", nullptr);
        fetch_headers_box_ =
            new BCheckBox("transfer-fetch-headers", "Fetch message headers only", nullptr);

        SetControlValue(send_box_, sending_only ? true : settings.send_on_check);
        SetControlValue(retrieve_new_box_, !sending_only);
        SetControlValue(delete_marked_box_, false);
        SetControlValue(retrieve_marked_box_, false);
        SetControlValue(delete_retrieved_box_, !settings.leave_mail_on_server && !sending_only);
        SetControlValue(delete_all_box_, false);
        SetControlValue(fetch_headers_box_, false);

        auto* persona_mode_menu = new BPopUpMenu("transfer-persona-mode");
        persona_mode_menu->SetRadioMode(true);
        persona_mode_menu->SetLabelFromMarked(true);
        auto* specified_item =
            new BMenuItem("Use the options below", new BMessage());
        auto* normal_item =
            new BMenuItem("Use normal send/check options", new BMessage());
        persona_mode_menu->AddItem(specified_item);
        persona_mode_menu->AddItem(normal_item);
        if (settings.transfer_persona_options == TransferPersonaOptionsMode::kSpecifiedOptions) {
            specified_item->SetMarked(true);
        } else {
            normal_item->SetMarked(true);
        }
        persona_mode_field_ = new BMenuField(
            "transfer-persona-field", "Selected personalities", persona_mode_menu);

        account_list_ = new BListView("transfer-accounts", B_MULTIPLE_SELECTION_LIST);
        for (const auto& account : accounts) {
            const std::string label =
                account.display_name.empty() ? account.id : account.display_name;
            account_list_->AddItem(new BStringItem(label.c_str()));
            account_ids_.push_back(account.id);
            if (account.check_mail_by_default) {
                account_list_->Select(account_list_->CountItems() - 1, true);
            }
        }
        if (account_list_->CountItems() == 0) {
            account_list_->AddItem(new BStringItem("(No personalities)"));
        }

        auto* account_scroll =
            new BScrollView("transfer-accounts-scroll", account_list_, 0, false, true);
        auto* cancel = new BButton("transfer-cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
        auto* ok = new BButton(
            "transfer-ok", "Transfer", new BMessage(kMailTransferOptionsAcceptedMessage));
        SetDefaultButton(ok);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(send_box_)
            .Add(retrieve_new_box_)
            .Add(delete_marked_box_)
            .Add(retrieve_marked_box_)
            .Add(delete_retrieved_box_)
            .Add(delete_all_box_)
            .Add(fetch_headers_box_)
            .Add(persona_mode_field_)
            .Add(new BStringView("transfer-accounts-label", "Personalities"))
            .Add(account_scroll)
            .AddGroup(B_HORIZONTAL, 8)
                .AddGlue()
                .Add(cancel)
                .Add(ok)
            .End();

        if (sending_only_) {
            retrieve_new_box_->SetEnabled(false);
        }
    }

    void MessageReceived(BMessage* message) override {
        if (message->what == kMailTransferOptionsAcceptedMessage) {
            BMessage payload(kMailTransferOptionsAcceptedMessage);
            payload.AddBool("send_queued", IsControlChecked(send_box_));
            payload.AddBool("retrieve_new", IsControlChecked(retrieve_new_box_));
            payload.AddBool("delete_marked", IsControlChecked(delete_marked_box_));
            payload.AddBool("retrieve_marked", IsControlChecked(retrieve_marked_box_));
            payload.AddBool("delete_retrieved", IsControlChecked(delete_retrieved_box_));
            payload.AddBool("delete_all", IsControlChecked(delete_all_box_));
            payload.AddBool("fetch_headers", IsControlChecked(fetch_headers_box_));
            payload.AddBool("sending_only", sending_only_);
            payload.AddInt32("transfer_persona_options", SelectedPersonaOption());
            for (int32 index = 0; index < account_list_->CountItems() &&
                                  index < static_cast<int32>(account_ids_.size());
                 ++index) {
                if (account_list_->IsItemSelected(index)) {
                    payload.AddString("account_id", account_ids_[static_cast<std::size_t>(index)].c_str());
                }
            }
            target_.SendMessage(&payload);
            PostMessage(B_QUIT_REQUESTED);
            return;
        }
        BWindow::MessageReceived(message);
    }

private:
    static void SetControlValue(BCheckBox* checkbox, bool value) {
        if (checkbox != nullptr) {
            checkbox->SetValue(value ? B_CONTROL_ON : B_CONTROL_OFF);
        }
    }

    static bool IsControlChecked(BCheckBox* checkbox) {
        return checkbox != nullptr && checkbox->Value() == B_CONTROL_ON;
    }

    int32 SelectedPersonaOption() const {
        if (persona_mode_field_ == nullptr || persona_mode_field_->Menu() == nullptr) {
            return 1;
        }
        for (int32 index = 0; index < persona_mode_field_->Menu()->CountItems(); ++index) {
            if (auto* item = persona_mode_field_->Menu()->ItemAt(index); item != nullptr &&
                item->IsMarked()) {
                return index;
            }
        }
        return 1;
    }

    std::vector<std::string> account_ids_;
    BMessenger target_;
    bool sending_only_ = false;
    BCheckBox* send_box_ = nullptr;
    BCheckBox* retrieve_new_box_ = nullptr;
    BCheckBox* delete_marked_box_ = nullptr;
    BCheckBox* retrieve_marked_box_ = nullptr;
    BCheckBox* delete_retrieved_box_ = nullptr;
    BCheckBox* delete_all_box_ = nullptr;
    BCheckBox* fetch_headers_box_ = nullptr;
    BMenuField* persona_mode_field_ = nullptr;
    BListView* account_list_ = nullptr;
};

class OptionsWindow final : public BWindow {
public:
    OptionsWindow(const ShellBehaviorSettings& shell_behavior,
                  const MailTransferSettings& mail_transfer,
                  const MailboxUiSettings& mailbox_ui,
                  const GuiPreferences& gui_preferences,
                  const BMessenger& target)
        : BWindow(BRect(0, 0, 620, 520),
                  "Options",
                  B_TITLED_WINDOW,
                  B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
          target_(target) {
        auto* tabs = new BTabView("options-tabs");

        offline_box_ = new BCheckBox("opt-offline", "Work Offline", nullptr);
        control_arrows_box_ = new BCheckBox("opt-control-arrows", "Cmd+Arrow navigates previous/next message", nullptr);
        alt_arrows_box_ = new BCheckBox("opt-alt-arrows", "Ctrl+Arrow navigates previous/next message", nullptr);
        backspace_delete_box_ =
            new BCheckBox("opt-backspace-delete", "Backspace deletes the selected message set", nullptr);
        reply_ctrl_r_to_all_box_ = new BCheckBox(
            "opt-reply-to-all", "Use Cmd+R for Reply All (Shift+Cmd+R replies only to sender)", nullptr);
        search_accel_switch_box_ = new BCheckBox("opt-search-switch",
                                                 "Swap Ctrl-F and Shift-Ctrl-F between Find Messages and text Find",
                                                 nullptr);
        SetControlValue(offline_box_, shell_behavior.offline);
        SetControlValue(control_arrows_box_, shell_behavior.control_arrows);
        SetControlValue(alt_arrows_box_, shell_behavior.alt_arrows);
        SetControlValue(backspace_delete_box_, shell_behavior.backspace_delete);
        SetControlValue(reply_ctrl_r_to_all_box_, shell_behavior.reply_ctrl_r_to_all);
        SetControlValue(search_accel_switch_box_, mailbox_ui.search_accel_switch);
        auto* connection_page = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(connection_page, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(offline_box_)
            .Add(control_arrows_box_)
            .Add(alt_arrows_box_)
            .Add(backspace_delete_box_)
            .Add(reply_ctrl_r_to_all_box_)
            .Add(search_accel_switch_box_);
        tabs->AddTab(connection_page)->SetLabel("Connection");

        show_status_box_ = new BCheckBox("opt-show-status", "Show Status column", nullptr);
        show_junk_box_ = new BCheckBox("opt-show-junk", "Show Junk column", nullptr);
        show_label_box_ = new BCheckBox("opt-show-label", "Show Label column", nullptr);
        show_server_status_box_ = new BCheckBox("opt-show-server", "Show Server Status column", nullptr);
        show_mailbox_lines_box_ = new BCheckBox("opt-mailbox-lines", "Draw mailbox summary lines", nullptr);
        black_lines_box_ = new BCheckBox("opt-black-lines", "Use black mailbox summary lines", nullptr);
        whole_label_color_box_ = new BCheckBox("opt-whole-label-color", "Use whole-summary label colors", nullptr);
        compose_italic_box_ = new BCheckBox("opt-compose-italic", "Italicize outgoing/queued summaries", nullptr);
        SetControlValue(show_status_box_, mailbox_ui.mailbox_show_status);
        SetControlValue(show_junk_box_, mailbox_ui.mailbox_show_junk);
        SetControlValue(show_label_box_, mailbox_ui.mailbox_show_label);
        SetControlValue(show_server_status_box_, mailbox_ui.mailbox_show_server_status);
        SetControlValue(show_mailbox_lines_box_, mailbox_ui.show_mailbox_lines);
        SetControlValue(black_lines_box_, mailbox_ui.black_toc_lines);
        SetControlValue(whole_label_color_box_, mailbox_ui.whole_summary_label_color);
        SetControlValue(compose_italic_box_, mailbox_ui.comp_summary_italic);
        auto* mailbox_page = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(mailbox_page, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(show_status_box_)
            .Add(show_junk_box_)
            .Add(show_label_box_)
            .Add(show_server_status_box_)
            .Add(show_mailbox_lines_box_)
            .Add(black_lines_box_)
            .Add(whole_label_color_box_)
            .Add(compose_italic_box_);
        tabs->AddTab(mailbox_page)->SetLabel("Mailbox");

        mark_previewed_read_box_ = new BCheckBox("opt-preview-read", "Mark previewed messages as read", nullptr);
        show_toolbar_tips_box_ = new BCheckBox("opt-toolbar-tips", "Show toolbar tips", nullptr);
        show_large_toolbar_box_ = new BCheckBox("opt-large-toolbar", "Use large toolbar buttons", nullptr);
        always_enable_junk_box_ = new BCheckBox("opt-always-junk", "Always enable Junk / Not Junk", nullptr);
        delete_fetched_junk_box_ = new BCheckBox("opt-delete-fetched-junk", "Delete fetched junk from server", nullptr);
        multiple_replies_box_ = new BCheckBox("opt-multi-replies", "Allow unrestricted multiple replies", nullptr);
        immediate_send_box_ = new BCheckBox("opt-immediate-send", "Immediate send is the default compose action", nullptr);
        send_on_check_box_ = new BCheckBox("opt-send-on-check", "Check Mail also sends queued mail", nullptr);
        leave_mail_on_server_box_ = new BCheckBox("opt-leave-on-server", "Leave POP mail on the server", nullptr);
        preview_read_seconds_ = new BTextControl("opt-preview-seconds", "Preview Read Delay:", "", nullptr);
        multi_reply_threshold_ = new BTextControl("opt-multi-threshold", "Reply Warning Threshold:", "", nullptr);
        SetControlValue(mark_previewed_read_box_, gui_preferences.mark_previewed_read);
        SetControlValue(show_toolbar_tips_box_, gui_preferences.show_toolbar_tips);
        SetControlValue(show_large_toolbar_box_, gui_preferences.show_toolbar_large_buttons);
        SetControlValue(always_enable_junk_box_, mailbox_ui.always_enable_junk);
        SetControlValue(delete_fetched_junk_box_, mailbox_ui.delete_fetched_junk);
        SetControlValue(multiple_replies_box_, mailbox_ui.multiple_replies_for_multiple_selections);
        SetControlValue(immediate_send_box_, mail_transfer.immediate_send);
        SetControlValue(send_on_check_box_, mail_transfer.send_on_check);
        SetControlValue(leave_mail_on_server_box_, mail_transfer.leave_mail_on_server);
        preview_read_seconds_->SetText(std::to_string(gui_preferences.preview_read_seconds).c_str());
        multi_reply_threshold_->SetText(std::to_string(mailbox_ui.multiple_reply_warn_threshold).c_str());
        auto* behavior_page = new BGroupView(B_VERTICAL);
        BLayoutBuilder::Group<>(behavior_page, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(mark_previewed_read_box_)
            .Add(preview_read_seconds_)
            .Add(show_toolbar_tips_box_)
            .Add(show_large_toolbar_box_)
            .Add(immediate_send_box_)
            .Add(send_on_check_box_)
            .Add(leave_mail_on_server_box_)
            .Add(always_enable_junk_box_)
            .Add(delete_fetched_junk_box_)
            .Add(multiple_replies_box_)
            .Add(multi_reply_threshold_);
        tabs->AddTab(behavior_page)->SetLabel("Behavior");

        auto* advanced_page = new BGroupView(B_VERTICAL);
        advanced_text_ = new BTextView("options-advanced");
        advanced_text_->MakeEditable(false);
        advanced_text_->SetWordWrap(true);
        advanced_text_->SetInsets(8, 8, 8, 8);
        std::ostringstream raw;
        raw << "Current imported settings snapshot\n\n";
        raw << "Offline=" << (shell_behavior.offline ? "1" : "0") << '\n';
        raw << "ControlArrows=" << (shell_behavior.control_arrows ? "1" : "0") << '\n';
        raw << "AltArrows=" << (shell_behavior.alt_arrows ? "1" : "0") << '\n';
        raw << "BackspaceDelete=" << (shell_behavior.backspace_delete ? "1" : "0") << '\n';
        raw << "ReplyToAll=" << (shell_behavior.reply_ctrl_r_to_all ? "1" : "0") << '\n';
        raw << "MailboxShowJunk=" << (mailbox_ui.mailbox_show_junk ? "1" : "0") << '\n';
        raw << "SearchAccelSwitch=" << (mailbox_ui.search_accel_switch ? "1" : "0") << '\n';
        raw << "ImmediateSend=" << (mail_transfer.immediate_send ? "1" : "0") << '\n';
        raw << "SendOnCheck=" << (mail_transfer.send_on_check ? "1" : "0") << '\n';
        raw << "LeaveMailOnServer=" << (mail_transfer.leave_mail_on_server ? "1" : "0") << '\n';
        raw << "TransferPersonaOptions="
            << (mail_transfer.transfer_persona_options == TransferPersonaOptionsMode::kSpecifiedOptions ? "0" : "1")
            << '\n';
        raw << "ToolTips=" << (gui_preferences.show_toolbar_tips ? "1" : "0") << '\n';
        raw << "ShowLargeButtons=" << (gui_preferences.show_toolbar_large_buttons ? "1" : "0") << '\n';
        raw << "Labels:\n";
        for (const auto& label : mailbox_ui.labels) {
            raw << "  " << label.index << ": " << label.name << " (" << label.red << ',' << label.green << ','
                << label.blue << ")\n";
        }
        advanced_text_->SetText(raw.str().c_str());
        BLayoutBuilder::Group<>(advanced_page, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(new BScrollView("options-advanced-scroll", advanced_text_, 0, false, true));
        tabs->AddTab(advanced_page)->SetLabel("Advanced");

        auto* cancel = new BButton("options-cancel", "Cancel", new BMessage(B_QUIT_REQUESTED));
        auto* ok = new BButton("options-ok", "Apply", new BMessage(kOptionsAcceptedMessage));
        SetDefaultButton(ok);

        BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .Add(tabs)
            .AddGroup(B_HORIZONTAL, 8)
                .AddGlue()
                .Add(cancel)
                .Add(ok)
            .End();
    }

    void MessageReceived(BMessage* message) override {
        if (message->what == kOptionsAcceptedMessage) {
            BMessage payload(kOptionsAcceptedMessage);
            payload.AddBool("offline", IsControlChecked(offline_box_));
            payload.AddBool("control_arrows", IsControlChecked(control_arrows_box_));
            payload.AddBool("alt_arrows", IsControlChecked(alt_arrows_box_));
            payload.AddBool("backspace_delete", IsControlChecked(backspace_delete_box_));
            payload.AddBool("reply_ctrl_r_to_all", IsControlChecked(reply_ctrl_r_to_all_box_));
            payload.AddBool("search_accel_switch", IsControlChecked(search_accel_switch_box_));
            payload.AddBool("mailbox_show_status", IsControlChecked(show_status_box_));
            payload.AddBool("mailbox_show_junk", IsControlChecked(show_junk_box_));
            payload.AddBool("mailbox_show_label", IsControlChecked(show_label_box_));
            payload.AddBool("mailbox_show_server_status", IsControlChecked(show_server_status_box_));
            payload.AddBool("show_mailbox_lines", IsControlChecked(show_mailbox_lines_box_));
            payload.AddBool("black_toc_lines", IsControlChecked(black_lines_box_));
            payload.AddBool("whole_summary_label_color", IsControlChecked(whole_label_color_box_));
            payload.AddBool("comp_summary_italic", IsControlChecked(compose_italic_box_));
            payload.AddBool("mark_previewed_read", IsControlChecked(mark_previewed_read_box_));
            payload.AddBool("show_toolbar_tips", IsControlChecked(show_toolbar_tips_box_));
            payload.AddBool("show_toolbar_large_buttons", IsControlChecked(show_large_toolbar_box_));
            payload.AddBool("immediate_send", IsControlChecked(immediate_send_box_));
            payload.AddBool("send_on_check", IsControlChecked(send_on_check_box_));
            payload.AddBool("leave_mail_on_server", IsControlChecked(leave_mail_on_server_box_));
            payload.AddBool("always_enable_junk", IsControlChecked(always_enable_junk_box_));
            payload.AddBool("delete_fetched_junk", IsControlChecked(delete_fetched_junk_box_));
            payload.AddBool("multiple_replies_for_multiple_selections", IsControlChecked(multiple_replies_box_));
            payload.AddInt32("preview_read_seconds", std::max(0, std::atoi(preview_read_seconds_->Text())));
            payload.AddInt32("multiple_reply_warn_threshold",
                             std::max(1, std::atoi(multi_reply_threshold_->Text())));
            target_.SendMessage(&payload);
            PostMessage(B_QUIT_REQUESTED);
            return;
        }
        BWindow::MessageReceived(message);
    }

private:
    static void SetControlValue(BCheckBox* checkbox, bool value) {
        if (checkbox != nullptr) {
            checkbox->SetValue(value ? B_CONTROL_ON : B_CONTROL_OFF);
        }
    }

    static bool IsControlChecked(BCheckBox* checkbox) {
        return checkbox != nullptr && checkbox->Value() == B_CONTROL_ON;
    }

    BMessenger target_;
    BCheckBox* offline_box_ = nullptr;
    BCheckBox* control_arrows_box_ = nullptr;
    BCheckBox* alt_arrows_box_ = nullptr;
    BCheckBox* backspace_delete_box_ = nullptr;
    BCheckBox* reply_ctrl_r_to_all_box_ = nullptr;
    BCheckBox* search_accel_switch_box_ = nullptr;
    BCheckBox* show_status_box_ = nullptr;
    BCheckBox* show_junk_box_ = nullptr;
    BCheckBox* show_label_box_ = nullptr;
    BCheckBox* show_server_status_box_ = nullptr;
    BCheckBox* show_mailbox_lines_box_ = nullptr;
    BCheckBox* black_lines_box_ = nullptr;
    BCheckBox* whole_label_color_box_ = nullptr;
    BCheckBox* compose_italic_box_ = nullptr;
    BCheckBox* mark_previewed_read_box_ = nullptr;
    BCheckBox* show_toolbar_tips_box_ = nullptr;
    BCheckBox* show_large_toolbar_box_ = nullptr;
    BCheckBox* immediate_send_box_ = nullptr;
    BCheckBox* send_on_check_box_ = nullptr;
    BCheckBox* leave_mail_on_server_box_ = nullptr;
    BCheckBox* always_enable_junk_box_ = nullptr;
    BCheckBox* delete_fetched_junk_box_ = nullptr;
    BCheckBox* multiple_replies_box_ = nullptr;
    BTextControl* preview_read_seconds_ = nullptr;
    BTextControl* multi_reply_threshold_ = nullptr;
    BTextView* advanced_text_ = nullptr;
};

class TocMessageRow final : public BRow {
public:
    TocMessageRow(const MessageSummary& summary,
                  const MailboxUiSettings& mailbox_ui,
                  const TocRowStyle& style)
        : BRow(std::max(kMessageItemMinHeight, 22.0f)),
          summary(summary),
          style(style),
          message_id(summary.id),
          searchable_text(ToLower(FormatStatusField(summary) + " " + FormatJunkScoreField(summary) + " " +
                                  FormatLabelField(summary, mailbox_ui) + " " +
                                  FormatPopServerStatusField(summary) + " " + summary.sender + " " +
                                  summary.subject + " " + summary.preview)) {
        const std::string attachment_text =
            summary.attachment_count == 0 ? "" : std::to_string(summary.attachment_count);
        SetField(new TocStringField(FormatStatusField(summary).c_str(), this, style), kTocFieldStatus);
        SetField(new TocStringField(FormatJunkScoreField(summary).c_str(), this, style), kTocFieldJunk);
        SetField(new TocStringField(FormatLabelField(summary, mailbox_ui).c_str(), this, style), kTocFieldLabel);
        SetField(new TocStringField(summary.priority.c_str(), this, style), kTocFieldPriority);
        SetField(new TocStringField(attachment_text.c_str(), this, style), kTocFieldAttachment);
        SetField(new TocStringField(summary.sender.c_str(), this, style), kTocFieldFrom);
        SetField(new TocStringField(FormatTimestamp(summary.timestamp).c_str(), this, style), kTocFieldDate);
        SetField(new TocStringField(FormatMessageSize(summary.size).c_str(), this, style), kTocFieldSize);
        SetField(new TocStringField(FormatPopServerStatusField(summary).c_str(), this, style),
                 kTocFieldPopServerStatus);
        SetField(new TocStringField(summary.download_complete && !summary.attachments_omitted ? "Complete" : "Partial",
                                    this,
                                    style),
                 kTocFieldDownload);
        SetField(new TocStringField(summary.subject.empty() ? "(No subject)" : summary.subject.c_str(), this, style),
                 kTocFieldSubject);
    }

    MessageSummary summary;
    TocRowStyle style;
    std::string message_id;
    std::string searchable_text;
};

void TocStyledColumn::DrawField(BField* field, BRect rect, BView* parent) {
    const auto* toc_field = dynamic_cast<const TocStringField*>(field);
    if (toc_field == nullptr) {
        BStringColumn::DrawField(field, rect, parent);
        return;
    }

    BFont original_font;
    parent->GetFont(&original_font);
    BFont styled_font(original_font);
    if (toc_field->Style().italic) {
        styled_font.SetFace(original_font.Face() | B_ITALIC_FACE);
        parent->SetFont(&styled_font, B_FONT_FACE);
    }

    const rgb_color original_color = parent->HighColor();
    if (toc_field->Style().use_label_color) {
        parent->SetHighColor(toc_field->Style().label_color);
    }

    BStringColumn::DrawField(field, rect, parent);

    if (toc_field->Style().use_label_color) {
        parent->SetHighColor(original_color);
    }
    if (toc_field->Style().italic) {
        parent->SetFont(&original_font, B_FONT_FACE);
    }
    if (toc_field->Style().draw_bottom_line) {
        parent->SetHighColor(toc_field->Style().line_color);
        parent->StrokeLine(BPoint(rect.left, rect.bottom), BPoint(rect.right, rect.bottom));
        parent->SetHighColor(original_color);
    }
}

int TocStyledColumn::CompareRows(const TocMessageRow& left, const TocMessageRow& right) const {
    switch (field_index_) {
        case kTocFieldStatus: {
            const int left_rank = LegacyMessageStatusSortRank(left.summary.legacy_status);
            const int right_rank = LegacyMessageStatusSortRank(right.summary.legacy_status);
            return (left_rank < right_rank) ? -1 : (left_rank > right_rank ? 1 : 0);
        }
        case kTocFieldJunk:
            return (left.summary.junk_score < right.summary.junk_score)
                       ? -1
                       : (left.summary.junk_score > right.summary.junk_score ? 1 : 0);
        case kTocFieldLabel:
            return (left.summary.label_index < right.summary.label_index)
                       ? -1
                       : (left.summary.label_index > right.summary.label_index ? 1 : 0);
        case kTocFieldPriority: {
            const int left_rank = PrioritySortRank(left.summary.priority);
            const int right_rank = PrioritySortRank(right.summary.priority);
            return (left_rank < right_rank) ? -1 : (left_rank > right_rank ? 1 : 0);
        }
        case kTocFieldAttachment:
            return (left.summary.attachment_count < right.summary.attachment_count)
                       ? -1
                       : (left.summary.attachment_count > right.summary.attachment_count ? 1 : 0);
        case kTocFieldFrom:
            return ToLower(left.summary.sender).compare(ToLower(right.summary.sender));
        case kTocFieldDate:
            return (left.summary.timestamp < right.summary.timestamp)
                       ? -1
                       : (left.summary.timestamp > right.summary.timestamp ? 1 : 0);
        case kTocFieldSize:
            return (left.summary.size < right.summary.size) ? -1 : (left.summary.size > right.summary.size ? 1 : 0);
        case kTocFieldPopServerStatus: {
            const int left_rank = PopServerStatusSortRank(left.summary.pop_server_status);
            const int right_rank = PopServerStatusSortRank(right.summary.pop_server_status);
            return (left_rank < right_rank) ? -1 : (left_rank > right_rank ? 1 : 0);
        }
        case kTocFieldDownload: {
            const int left_rank = (left.summary.download_complete && !left.summary.attachments_omitted) ? 1 : 0;
            const int right_rank = (right.summary.download_complete && !right.summary.attachments_omitted) ? 1 : 0;
            return (left_rank < right_rank) ? -1 : (left_rank > right_rank ? 1 : 0);
        }
        case kTocFieldSubject:
            return CompareMailboxSubjects(left.summary.subject, right.summary.subject);
        default:
            return 0;
    }
}

int TocStyledColumn::CompareFields(BField* field1, BField* field2) {
    const auto* left_field = dynamic_cast<const TocStringField*>(field1);
    const auto* right_field = dynamic_cast<const TocStringField*>(field2);
    if (left_field == nullptr || right_field == nullptr || left_field->Row() == nullptr || right_field->Row() == nullptr) {
        return BStringColumn::CompareFields(field1, field2);
    }

    const int primary = CompareRows(*left_field->Row(), *right_field->Row());
    if (primary != 0) {
        return primary;
    }

    const int unread_tiebreak =
        (left_field->Row()->summary.unread == right_field->Row()->summary.unread)
            ? 0
            : (left_field->Row()->summary.unread ? -1 : 1);
    if (unread_tiebreak != 0) {
        return unread_tiebreak;
    }

    if (left_field->Row()->summary.timestamp != right_field->Row()->summary.timestamp) {
        return left_field->Row()->summary.timestamp < right_field->Row()->summary.timestamp ? -1 : 1;
    }

    const int subject_compare =
        CompareMailboxSubjects(left_field->Row()->summary.subject, right_field->Row()->summary.subject);
    if (subject_compare != 0) {
        return subject_compare;
    }

    return ToLower(left_field->Row()->message_id).compare(ToLower(right_field->Row()->message_id));
}

std::string PriorityMenuLabel(ComposePriority priority) {
    switch (priority) {
        case ComposePriority::kHighest:
            return "Highest";
        case ComposePriority::kHigh:
            return "High";
        case ComposePriority::kNormal:
            return "Normal";
        case ComposePriority::kLow:
            return "Low";
        case ComposePriority::kLowest:
            return "Lowest";
    }
    return "Normal";
}

LegacyMessageStatus LegacyStatusFromInt(int value) {
    switch (value) {
        case 0:
            return LegacyMessageStatus::kUnread;
        case 1:
            return LegacyMessageStatus::kRead;
        case 2:
            return LegacyMessageStatus::kReplied;
        case 3:
            return LegacyMessageStatus::kForwarded;
        case 4:
            return LegacyMessageStatus::kRedirected;
        case 5:
            return LegacyMessageStatus::kRecovered;
        case 6:
            return LegacyMessageStatus::kSendable;
        case 7:
            return LegacyMessageStatus::kQueued;
        case 8:
            return LegacyMessageStatus::kTimeQueued;
        case 9:
            return LegacyMessageStatus::kSent;
        case 10:
            return LegacyMessageStatus::kUnsent;
        default:
            return LegacyMessageStatus::kRead;
    }
}

std::string ExtractEmailAddress(std::string_view value) {
    const std::size_t lt = value.find('<');
    const std::size_t gt = value.find('>', lt == std::string_view::npos ? 0 : lt + 1);
    if (lt != std::string_view::npos && gt != std::string_view::npos && gt > lt + 1) {
        return std::string(value.substr(lt + 1, gt - lt - 1));
    }
    return std::string(value);
}

std::string ExtractDisplayName(std::string_view value) {
    const std::size_t lt = value.find('<');
    if (lt != std::string_view::npos) {
        return std::string(value.substr(0, lt));
    }
    return std::string(value);
}

}  // namespace

HaikuMainWindow::HaikuMainWindow(HaikuShellHost& shell_host)
    : BWindow(BRect(100, 100, 1260, 900),
              hermes::kHemeraProductName.data(),
      B_TITLED_WINDOW,
      B_QUIT_ON_WINDOW_CLOSE | B_AUTO_UPDATE_SIZE_LIMITS),
      shell_host_(shell_host),
      gui_preferences_(GuiPreferencesFromSettings(shell_host.Settings())),
      mailbox_ui_(shell_host.MailboxUi()) {
    const auto register_command_item = [this](uint32 command, BMenuItem* item) -> BMenuItem* {
        if (item != nullptr) {
            command_menu_items_[command].push_back(item);
        }
        return item;
    };

    auto* menu_bar = new BMenuBar("menu-bar");

    auto* file_menu = new BMenu("File");
    file_menu->AddItem(new BMenuItem("New Message", new BMessage(kNewComposeMessage)));
    file_menu->AddSeparatorItem();
    file_menu->AddItem(new BMenuItem("Import from Eudora" B_UTF8_ELLIPSIS,
                                     new BMessage(kImportFromEudoraMessage)));
    file_menu->AddSeparatorItem();
    file_menu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED)));
    menu_bar->AddItem(file_menu);

    auto* mail_menu = new BMenu("Mail");
    mail_menu->AddItem(register_command_item(kCheckMailMessage, new BMenuItem("Check Mail", new BMessage(kCheckMailMessage))));
    mail_menu->AddItem(register_command_item(kSendQueuedMessage, new BMenuItem("Send Queued", new BMessage(kSendQueuedMessage))));
    mail_menu->AddItem(register_command_item(kSendReceiveMessage, new BMenuItem("Send & Receive", new BMessage(kSendReceiveMessage))));
    mail_menu->AddItem(register_command_item(kStopTasksMessage, new BMenuItem("Stop Tasks", new BMessage(kStopTasksMessage))));
    menu_bar->AddItem(mail_menu);

    auto* special_menu = new BMenu("Special");
    work_offline_item_ =
        register_command_item(kSpecialWorkOfflineMessage,
                              new BMenuItem("Work Offline", new BMessage(kSpecialWorkOfflineMessage)));
    special_menu->AddItem(work_offline_item_);
    special_menu->AddSeparatorItem();
    special_menu->AddItem(register_command_item(kSpecialEmptyTrashMessage,
                                                new BMenuItem("Empty Trash", new BMessage(kSpecialEmptyTrashMessage))));
    special_menu->AddItem(register_command_item(
        kSpecialTrimJunkMessage,
        new BMenuItem("Delete Old Junk", new BMessage(kSpecialTrimJunkMessage))));
    special_menu->AddItem(register_command_item(kSpecialCompactMailboxesMessage,
                                                new BMenuItem("Compact Mailboxes",
                                                              new BMessage(kSpecialCompactMailboxesMessage))));
    special_menu->AddSeparatorItem();
    special_menu->AddItem(register_command_item(kSpecialForgetPasswordsMessage,
                                                new BMenuItem("Forget Password(s)",
                                                              new BMessage(kSpecialForgetPasswordsMessage))));
    special_menu->AddItem(register_command_item(kSpecialChangePasswordMessage,
                                                new BMenuItem("Change Password" B_UTF8_ELLIPSIS,
                                                              new BMessage(kSpecialChangePasswordMessage))));
    special_menu->AddSeparatorItem();
    special_menu->AddItem(register_command_item(kSpecialOptionsMessage,
                                                new BMenuItem("Options" B_UTF8_ELLIPSIS,
                                                              new BMessage(kSpecialOptionsMessage))));
    menu_bar->AddItem(special_menu);

    edit_menu_ = new BMenu("Edit");
    find_text_item_ =
        register_command_item(kFindTextMessage,
                              new BMenuItem("Find" B_UTF8_ELLIPSIS, new BMessage(kFindTextMessage)));
    edit_menu_->AddItem(find_text_item_);
    find_again_item_ = register_command_item(kFindAgainMessage, new BMenuItem("Find Again", new BMessage(kFindAgainMessage)));
    if (find_again_item_ != nullptr) {
        find_again_item_->SetShortcut(B_F3_KEY, 0);
    }
    edit_menu_->AddItem(find_again_item_);
    menu_bar->AddItem(edit_menu_);

    auto* view_menu = new BMenu("View");
    show_toolbar_item_ = new BMenuItem("Show Toolbar", new BMessage(kToggleToolbarMessage));
    show_search_bar_item_ = new BMenuItem("Show Search Bar", new BMessage(kToggleSearchBarMessage));
    show_preview_item_ = new BMenuItem("Show Preview Pane", new BMessage(kTogglePreviewPaneMessage));
    show_utility_item_ = new BMenuItem("Show Tasks Window", new BMessage(kToggleUtilityPaneMessage));
    view_menu->AddItem(show_toolbar_item_);
    view_menu->AddItem(show_search_bar_item_);
    view_menu->AddItem(show_preview_item_);
    view_menu->AddItem(show_utility_item_);
    view_menu->AddSeparatorItem();
    group_by_subject_item_ = new BMenuItem("Group by Subject", new BMessage(kGroupBySubjectMessage));
    view_menu->AddItem(group_by_subject_item_);
    view_menu->AddSeparatorItem();
    view_menu->AddItem(new BMenuItem("Task Status", new BMessage(kSelectTaskStatusTabMessage)));
    view_menu->AddItem(new BMenuItem("Task Errors", new BMessage(kSelectTaskErrorsTabMessage)));
    view_menu->AddSeparatorItem();
    view_menu->AddItem(new BMenuItem("Customize Toolbar" B_UTF8_ELLIPSIS,
                                     new BMessage(kCustomizeToolbarMessage)));
    menu_bar->AddItem(view_menu);

    auto* tools_menu = new BMenu("Tools");
    const std::vector<std::pair<const char*, const char*>> tool_entries = {
        {"Mailboxes", "mailboxes"},
        {"Task Status", "task-status"},
        {"Task Errors", "task-errors"},
        {"Signatures", "signatures"},
        {"Stationery", "stationery"},
        {"Nicknames", "nicknames"},
        {"Personalities", "personalities"},
        {"Filters", "filters"},
        {"Filter Report", "filter-report"},
        {"Directory Services", "directory-services"},
        {"File Browser", "file-browser"},
        {"Link History", "link-history"},
        {"Search", "search"},
        {"Plugins", "plugins"},
    };
    for (const auto& tool_entry : tool_entries) {
        auto* item_message = new BMessage(kOpenToolWindowMessage);
        item_message->AddString("tool_id", tool_entry.second);
        tools_menu->AddItem(new BMenuItem(tool_entry.first, item_message));
    }
    menu_bar->AddItem(tools_menu);

    auto* window_menu = new BMenu("Window");
    for (const auto& tool_entry : tool_entries) {
        auto* item_message = new BMessage(kOpenToolWindowMessage);
        item_message->AddString("tool_id", tool_entry.second);
        window_menu->AddItem(new BMenuItem(tool_entry.first, item_message));
    }
    window_menu->AddSeparatorItem();
    window_menu->AddItem(register_command_item(
        kWindowSendBehindMessage, new BMenuItem("Send to Back", new BMessage(kWindowSendBehindMessage))));
    window_menu->AddItem(register_command_item(
        kWindowCascadeMessage, new BMenuItem("Cascade", new BMessage(kWindowCascadeMessage))));
    window_menu->AddItem(register_command_item(
        kWindowTileHorizontalMessage,
        new BMenuItem("Tile Horizontally", new BMessage(kWindowTileHorizontalMessage))));
    window_menu->AddItem(register_command_item(
        kWindowTileVerticalMessage,
        new BMenuItem("Tile Vertically", new BMessage(kWindowTileVerticalMessage))));
    window_menu->AddItem(register_command_item(
        kWindowArrangeMessage, new BMenuItem("Arrange", new BMessage(kWindowArrangeMessage))));
    window_menu->AddItem(register_command_item(
        kWindowCloseAllMessage, new BMenuItem("Close All", new BMessage(kWindowCloseAllMessage))));
    window_menu->AddSeparatorItem();
    window_menu->AddItem(new BMenuItem("Help Contents", new BMessage(kOpenHelpContentsMessage)));
    window_menu->AddItem(new BMenuItem("Import from Eudora" B_UTF8_ELLIPSIS,
                                       new BMessage(kImportFromEudoraMessage)));
    menu_bar->AddItem(window_menu);

    auto* mailbox_menu = new BMenu("Mailbox");
    mailbox_menu->AddItem(
        register_command_item(kRefreshMailboxMessage, new BMenuItem("Refresh", new BMessage(kRefreshMailboxMessage))));
    mailbox_menu->AddItem(
        register_command_item(kResyncMailboxMessage, new BMenuItem("Resync", new BMessage(kResyncMailboxMessage))));
    mailbox_menu->AddSeparatorItem();
    recent_mailboxes_menu_ = new BMenu("Recent");
    mailbox_menu->AddItem(recent_mailboxes_menu_);
    mailbox_menu->AddSeparatorItem();
    mailbox_menu->AddItem(new BMenuItem("Create Remote Mailbox", new BMessage(kCreateMailboxMessage)));
    mailbox_menu->AddItem(new BMenuItem("Rename Remote Mailbox", new BMessage(kRenameMailboxMessage)));
    mailbox_menu->AddItem(new BMenuItem("Delete Remote Mailbox", new BMessage(kDeleteMailboxMessage)));
    menu_bar->AddItem(mailbox_menu);

    auto* message_menu = new BMenu("Message");
    message_menu->AddItem(
        register_command_item(kOpenMessageMessage, new BMenuItem("Open", new BMessage(kOpenMessageMessage))));
    find_messages_item_ =
        register_command_item(kFindMessagesMessage,
                              new BMenuItem("Find Messages" B_UTF8_ELLIPSIS,
                                            new BMessage(kFindMessagesMessage)));
    message_menu->AddItem(find_messages_item_);
    message_menu->AddSeparatorItem();
    reply_item_ =
        register_command_item(kReplyMessage, new BMenuItem("Reply", new BMessage(kReplyMessage)));
    message_menu->AddItem(reply_item_);
    reply_all_item_ =
        register_command_item(kReplyAllMessage, new BMenuItem("Reply All", new BMessage(kReplyAllMessage)));
    message_menu->AddItem(reply_all_item_);
    message_menu->AddItem(
        register_command_item(kForwardMessage, new BMenuItem("Forward", new BMessage(kForwardMessage))));
    message_menu->AddItem(
        register_command_item(kRedirectMessage, new BMenuItem("Redirect", new BMessage(kRedirectMessage))));
    message_menu->AddItem(
        register_command_item(kSendAgainMessage, new BMenuItem("Send Again", new BMessage(kSendAgainMessage))));
    send_immediately_item_ = register_command_item(
        kSendImmediatelyMessage, new BMenuItem("Send Immediately", new BMessage(kSendImmediatelyMessage)));
    message_menu->AddItem(send_immediately_item_);
    message_menu->AddItem(register_command_item(
        kChangeQueueingMessage, new BMenuItem("Change Queueing" B_UTF8_ELLIPSIS, new BMessage(kChangeQueueingMessage))));
    message_menu->AddItem(register_command_item(
        kPreviousMessageMessage, new BMenuItem("Previous Message", new BMessage(kPreviousMessageMessage))));
    message_menu->AddItem(register_command_item(
        kNextMessageMessage, new BMenuItem("Next Message", new BMessage(kNextMessageMessage))));
    message_menu->AddSeparatorItem();
    message_menu->AddItem(register_command_item(
        kDeleteMessageMessage, new BMenuItem("Delete", new BMessage(kDeleteMessageMessage))));
    message_menu->AddItem(register_command_item(
        kUndeleteMessageMessage, new BMenuItem("Undelete", new BMessage(kUndeleteMessageMessage))));
    message_menu->AddItem(
        register_command_item(kPurgeMailboxMessage, new BMenuItem("Purge/Expunge", new BMessage(kPurgeMailboxMessage))));
    message_menu->AddSeparatorItem();
    auto* status_menu = new BMenu("Status");
    auto add_status_item = [this, &register_command_item, status_menu](const char* label, LegacyMessageStatus status) {
        auto* message = new BMessage(kSetLegacyStatusMessage);
        message->AddInt32("legacy_status", static_cast<int32>(status));
        status_menu->AddItem(register_command_item(kSetLegacyStatusMessage, new BMenuItem(label, message)));
    };
    status_menu->AddItem(register_command_item(kMarkReadMessage, new BMenuItem("Mark Read", new BMessage(kMarkReadMessage))));
    status_menu->AddItem(register_command_item(kMarkUnreadMessage, new BMenuItem("Mark Unread", new BMessage(kMarkUnreadMessage))));
    status_menu->AddItem(register_command_item(kToggleStatusMessage,
                                               new BMenuItem("Toggle Read Status",
                                                             new BMessage(kToggleStatusMessage))));
    status_menu->AddSeparatorItem();
    add_status_item("Replied", LegacyMessageStatus::kReplied);
    add_status_item("Forwarded", LegacyMessageStatus::kForwarded);
    add_status_item("Redirected", LegacyMessageStatus::kRedirected);
    add_status_item("Unsendable", LegacyMessageStatus::kUnsendable);
    add_status_item("Recovered", LegacyMessageStatus::kRecovered);
    add_status_item("Sendable", LegacyMessageStatus::kSendable);
    add_status_item("Queued", LegacyMessageStatus::kQueued);
    add_status_item("Time Queued", LegacyMessageStatus::kTimeQueued);
    add_status_item("Sent", LegacyMessageStatus::kSent);
    add_status_item("Unsent", LegacyMessageStatus::kUnsent);
    message_menu->AddItem(status_menu);
    auto* label_menu = new BMenu("Label");
    auto add_label_item = [this, &register_command_item, label_menu](const std::string& label, int label_index) {
        auto* message = new BMessage(kSetLabelMessage);
        message->AddInt32("label_index", label_index);
        auto* item = register_command_item(kSetLabelMessage, new BMenuItem(label.c_str(), message));
        label_menu_items_.push_back(item);
        label_menu->AddItem(item);
    };
    add_label_item("No Label", 0);
    for (int index = 1; index <= 7; ++index) {
        add_label_item(MailboxLabelName(mailbox_ui_, index), index);
    }
    message_menu->AddItem(label_menu);
    auto* server_status_menu = new BMenu("Server Status");
    server_status_menu->AddItem(register_command_item(
        kServerLeaveMessage, new BMenuItem("Leave", new BMessage(kServerLeaveMessage))));
    server_status_menu->AddItem(register_command_item(
        kServerFetchMessage, new BMenuItem("Fetch", new BMessage(kServerFetchMessage))));
    server_status_menu->AddItem(register_command_item(
        kServerDeleteMessage, new BMenuItem("Delete", new BMessage(kServerDeleteMessage))));
    server_status_menu->AddItem(register_command_item(
        kServerFetchDeleteMessage,
        new BMenuItem("Fetch + Delete", new BMessage(kServerFetchDeleteMessage))));
    message_menu->AddItem(server_status_menu);
    auto* priority_menu = new BMenu("Priority");
    priority_menu->AddItem(
        register_command_item(kPriorityHighestMessage, new BMenuItem("Highest", new BMessage(kPriorityHighestMessage))));
    priority_menu->AddItem(
        register_command_item(kPriorityHighMessage, new BMenuItem("High", new BMessage(kPriorityHighMessage))));
    priority_menu->AddItem(
        register_command_item(kPriorityNormalMessage, new BMenuItem("Normal", new BMessage(kPriorityNormalMessage))));
    priority_menu->AddItem(
        register_command_item(kPriorityLowMessage, new BMenuItem("Low", new BMessage(kPriorityLowMessage))));
    priority_menu->AddItem(
        register_command_item(kPriorityLowestMessage, new BMenuItem("Lowest", new BMessage(kPriorityLowestMessage))));
    message_menu->AddItem(priority_menu);
    message_menu->AddSeparatorItem();
    message_menu->AddItem(
        register_command_item(kMoveMessageMessage, new BMenuItem("Move", new BMessage(kMoveMessageMessage))));
    message_menu->AddItem(
        register_command_item(kCopyMessageMessage, new BMenuItem("Copy", new BMessage(kCopyMessageMessage))));
    message_menu->AddSeparatorItem();
    message_menu->AddItem(
        register_command_item(kMarkJunkMessage, new BMenuItem("Junk", new BMessage(kMarkJunkMessage))));
    message_menu->AddItem(
        register_command_item(kMarkNotJunkMessage, new BMenuItem("Not Junk", new BMessage(kMarkNotJunkMessage))));
    message_menu->AddItem(
        register_command_item(kRecheckJunkMessage, new BMenuItem("Recheck Junk", new BMessage(kRecheckJunkMessage))));
    message_menu->AddSeparatorItem();
    message_menu->AddItem(
        register_command_item(kFilterMessagesMessage, new BMenuItem("Filter Messages", new BMessage(kFilterMessagesMessage))));
    message_menu->AddItem(
        register_command_item(kMakeFilterMessage, new BMenuItem("Make Filter", new BMessage(kMakeFilterMessage))));
    message_menu->AddSeparatorItem();
    message_menu->AddItem(register_command_item(
        kMakeNicknameMessage, new BMenuItem("Make Nickname", new BMessage(kMakeNicknameMessage))));
    new_message_to_menu_ = new BMenu("New Message To");
    message_menu->AddItem(new_message_to_menu_);
    forward_to_menu_ = new BMenu("Forward To");
    message_menu->AddItem(forward_to_menu_);
    redirect_to_menu_ = new BMenu("Redirect To");
    message_menu->AddItem(redirect_to_menu_);
    new_message_with_menu_ = new BMenu("New Message With");
    message_menu->AddItem(new_message_with_menu_);
    reply_with_menu_ = new BMenu("Reply With");
    message_menu->AddItem(reply_with_menu_);
    reply_all_with_menu_ = new BMenu("Reply to All With");
    message_menu->AddItem(reply_all_with_menu_);
    change_personality_menu_ = new BMenu("Change Personality");
    message_menu->AddItem(change_personality_menu_);
    message_menu->AddSeparatorItem();
    message_menu->AddItem(register_command_item(
        kPrintPreviewMessage, new BMenuItem("Print Preview", new BMessage(kPrintPreviewMessage))));
    message_menu->AddItem(
        register_command_item(kPrintDirectMessage, new BMenuItem("Print One", new BMessage(kPrintDirectMessage))));
    message_menu->AddSeparatorItem();
    auto* imap_menu = new BMenu("IMAP");
    imap_menu->AddItem(register_command_item(
        kFetchFullMessageMessage, new BMenuItem("Fetch Full", new BMessage(kFetchFullMessageMessage))));
    imap_menu->AddItem(register_command_item(
        kFetchDefaultMessageMessage, new BMenuItem("Fetch Default", new BMessage(kFetchDefaultMessageMessage))));
    imap_menu->AddSeparatorItem();
    imap_menu->AddItem(register_command_item(
        kImapRedownloadFullMessage, new BMenuItem("Redownload Full", new BMessage(kImapRedownloadFullMessage))));
    imap_menu->AddItem(register_command_item(
        kImapRedownloadDefaultMessage,
        new BMenuItem("Redownload Default", new BMessage(kImapRedownloadDefaultMessage))));
    imap_menu->AddSeparatorItem();
    imap_menu->AddItem(register_command_item(
        kImapClearCachedMessage, new BMenuItem("Clear Cached", new BMessage(kImapClearCachedMessage))));
    message_menu->AddItem(imap_menu);
    menu_bar->AddItem(message_menu);

    auto* attachment_menu = new BMenu("Attachments");
    attachment_menu->AddItem(new BMenuItem("Open", new BMessage(kOpenAttachmentMessage)));
    attachment_menu->AddItem(new BMenuItem("Save", new BMessage(kSaveAttachmentMessage)));
    attachment_menu->AddItem(new BMenuItem("Save All", new BMessage(kSaveAllAttachmentsMessage)));
    attachment_menu->AddSeparatorItem();
    attachment_menu->AddItem(new BMenuItem("Fetch", new BMessage(kFetchAttachmentMessage)));
    menu_bar->AddItem(attachment_menu);

    auto* task_menu = new BMenu("Tasks");
    task_menu->AddItem(new BMenuItem("Retry Selected Action", new BMessage(kRetryTaskMessage)));
    task_menu->AddItem(new BMenuItem("Cancel Selected Action", new BMessage(kCancelTaskMessage)));
    menu_bar->AddItem(task_menu);

    auto* help_menu = new BMenu("Help");
    help_menu->AddItem(new BMenuItem("Help Contents", new BMessage(kOpenHelpContentsMessage)));
    help_menu->AddItem(new BMenuItem("Reveal Help Files", new BMessage(kRevealHelpFilesMessage)));
    menu_bar->AddItem(help_menu);

    toolbar_view_ = new BToolBar(B_HORIZONTAL);
    RebuildToolbar();

    search_bar_mode_ = gui_preferences_.search_bar_mode;
    search_bar_scope_menu_ = new BPopUpMenu("main-search-scope");
    search_bar_scope_menu_->SetRadioMode(true);
    search_bar_scope_menu_->SetLabelFromMarked(true);
    auto* search_web_item = new BMenuItem("Search Web", new BMessage(kSearchBarScopeChangedMessage));
    search_web_item->Message()->AddInt32("scope_mode", static_cast<int32>(SearchBarMode::kSearchWeb));
    auto* search_all_item =
        new BMenuItem("Search Eudora", new BMessage(kSearchBarScopeChangedMessage));
    search_all_item->Message()->AddInt32(
        "scope_mode", static_cast<int32>(SearchBarMode::kSearchAllMailboxes));
    auto* search_current_item =
        new BMenuItem("Search Mailbox", new BMessage(kSearchBarScopeChangedMessage));
    search_current_item->Message()->AddInt32(
        "scope_mode", static_cast<int32>(SearchBarMode::kSearchCurrentMailbox));
    auto* search_folder_item =
        new BMenuItem("Search Mailfolder", new BMessage(kSearchBarScopeChangedMessage));
    search_folder_item->Message()->AddInt32(
        "scope_mode", static_cast<int32>(SearchBarMode::kSearchCurrentFolder));
    search_bar_scope_menu_->AddItem(search_web_item);
    search_bar_scope_menu_->AddItem(search_all_item);
    search_bar_scope_menu_->AddItem(search_current_item);
    search_bar_scope_menu_->AddItem(search_folder_item);
    search_bar_scope_menu_->SetTargetForItems(this);
    search_bar_scope_field_ = new BMenuField("main-search-scope-field", "", search_bar_scope_menu_);
    search_bar_recent_menu_ = new BPopUpMenu("Search");
    search_bar_recent_field_ = new BMenuField("main-search-recent-field", "", search_bar_recent_menu_);
    search_bar_query_control_ = new BTextControl("main-search-query", "", "", nullptr);
    search_bar_query_control_->SetExplicitMinSize(
        BSize(static_cast<float>(gui_preferences_.search_bar_width), B_SIZE_UNSET));
    if (auto* query_text = search_bar_query_control_->TextView()) {
        query_text->SetModificationMessage(new BMessage(kSearchBarQueryModifiedMessage));
        query_text->AddFilter(new SearchBarTextFilter(this));
    }
    search_bar_button_ = new BButton("main-search-go", "Search", new BMessage(kSearchBarExecuteMessage));
    search_bar_container_ = new BGroupView(B_HORIZONTAL);
    BLayoutBuilder::Group<>(search_bar_container_, B_HORIZONTAL, 8)
        .SetInsets(B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING, B_USE_WINDOW_SPACING, 0)
        .Add(new BStringView("main-search-label", "Search"))
        .Add(search_bar_scope_field_)
        .Add(search_bar_recent_field_)
        .Add(search_bar_query_control_, 1.0f)
        .Add(search_bar_button_);
    RefreshSearchBarRecentMenu();
    SetSearchBarQueryText(SearchBarPromptText(), true);

    status_view_ = new BStringView("main-status", "Ready.");

    message_list_ = new ContextColumnListView("messages",
                                              new BMessage(kMessageSelectedMessage),
                                              new BMessage(kOpenMessageMessage),
                                              [this](BPoint where) { ShowMessageContextMenu(where); },
                                              [this](BPoint where, uint32 modifiers) {
                                                  HandleTocHeaderClick(where, modifiers);
                                              });
    status_column_ =
        new TocStyledColumn("Status", kTocFieldStatus, 84.0f, 52.0f, 200.0f, B_TRUNCATE_END);
    message_list_->AddColumn(status_column_, kTocFieldStatus);
    toc_columns_.push_back({status_column_, kTocFieldStatus});
    junk_column_ = new TocStyledColumn("Junk", kTocFieldJunk, 72.0f, 48.0f, 120.0f, B_TRUNCATE_END, B_ALIGN_RIGHT);
    message_list_->AddColumn(junk_column_, kTocFieldJunk);
    toc_columns_.push_back({junk_column_, kTocFieldJunk});
    label_column_ =
        new TocStyledColumn("Label", kTocFieldLabel, 120.0f, 72.0f, 220.0f, B_TRUNCATE_END);
    message_list_->AddColumn(label_column_, kTocFieldLabel);
    toc_columns_.push_back({label_column_, kTocFieldLabel});
    auto* priority_column =
        new TocStyledColumn("Priority", kTocFieldPriority, 72.0f, 48.0f, 180.0f, B_TRUNCATE_END);
    message_list_->AddColumn(priority_column, kTocFieldPriority);
    toc_columns_.push_back({priority_column, kTocFieldPriority});
    auto* attachment_column = new TocStyledColumn(
        "Attach", kTocFieldAttachment, 64.0f, 44.0f, 96.0f, B_TRUNCATE_END, B_ALIGN_RIGHT);
    message_list_->AddColumn(attachment_column, kTocFieldAttachment);
    toc_columns_.push_back({attachment_column, kTocFieldAttachment});
    auto* from_column =
        new TocStyledColumn("From", kTocFieldFrom, 200.0f, 96.0f, 420.0f, B_TRUNCATE_END);
    message_list_->AddColumn(from_column, kTocFieldFrom);
    toc_columns_.push_back({from_column, kTocFieldFrom});
    auto* date_column =
        new TocStyledColumn("Date", kTocFieldDate, 156.0f, 108.0f, 220.0f, B_TRUNCATE_END);
    message_list_->AddColumn(date_column, kTocFieldDate);
    toc_columns_.push_back({date_column, kTocFieldDate});
    auto* size_column = new TocStyledColumn(
        "Size", kTocFieldSize, 92.0f, 64.0f, 160.0f, B_TRUNCATE_END, B_ALIGN_RIGHT);
    message_list_->AddColumn(size_column, kTocFieldSize);
    toc_columns_.push_back({size_column, kTocFieldSize});
    pop_server_status_column_ =
        new TocStyledColumn("Server", kTocFieldPopServerStatus, 112.0f, 84.0f, 220.0f, B_TRUNCATE_END);
    message_list_->AddColumn(pop_server_status_column_, kTocFieldPopServerStatus);
    toc_columns_.push_back({pop_server_status_column_, kTocFieldPopServerStatus});
    auto* download_column =
        new TocStyledColumn("Download", kTocFieldDownload, 108.0f, 92.0f, 220.0f, B_TRUNCATE_END);
    message_list_->AddColumn(download_column, kTocFieldDownload);
    toc_columns_.push_back({download_column, kTocFieldDownload});
    auto* subject_column =
        new TocStyledColumn("Subject", kTocFieldSubject, 360.0f, 140.0f, 640.0f, B_TRUNCATE_END);
    message_list_->AddColumn(subject_column, kTocFieldSubject);
    toc_columns_.push_back({subject_column, kTocFieldSubject});
    RestoreColumnListState(*message_list_, gui_preferences_.toc_column_layout);
    UpdateTocColumnVisibility();
    attachment_list_ = new ContextListView("attachments",
                                           new BMessage(kAttachmentSelectedMessage),
                                           [this](BPoint where) { ShowAttachmentContextMenu(where); });
    preview_subject_ = new BStringView("preview-subject", "Subject: ");
    preview_from_ = new BStringView("preview-from", "From: ");
    preview_to_ = new BStringView("preview-to", "To: ");
    preview_date_ = new BStringView("preview-date", "Date: ");
    preview_state_ = new BStringView("preview-state", "State: ");

    preview_text_ = new ContextTextView("preview", [this](BPoint where) { ShowPreviewContextMenu(where); });
    preview_text_->MakeEditable(false);
    preview_text_->SetWordWrap(true);
    preview_text_->SetInsets(kHeaderInset, kHeaderInset, kHeaderInset, kHeaderInset);
    preview_plain_root_ = new BScrollView("preview-scroll", preview_text_, 0, true, true);
    preview_web_view_ = new HaikuWebKitMessageView(shell_host_.DataRootPath() / "Cache" / "WebKit");
    preview_web_view_->SetContextMenuHandler([this](BPoint where) { ShowPreviewContextMenu(where); });
    preview_web_view_->Hide();

    preview_container_ = new BGroupView(B_VERTICAL);

    attachment_container_ = new BGroupView(B_VERTICAL);
    auto* attachment_heading = new BStringView("attachment-heading", "Attachments");
    auto* attachment_scroll =
        new BScrollView("attachments-scroll", attachment_list_, 0, false, true);
    BLayoutBuilder::Group<>(attachment_container_, B_VERTICAL, 6)
        .SetInsets(0, 0, 0, 0)
        .Add(attachment_heading)
        .Add(attachment_scroll);

    BLayoutBuilder::Group<>(preview_container_, B_VERTICAL, 6)
        .SetInsets(B_USE_SMALL_SPACING)
        .Add(preview_subject_)
        .Add(preview_from_)
        .Add(preview_to_)
        .Add(preview_date_)
        .Add(preview_state_)
        .Add(preview_plain_root_)
        .Add(preview_web_view_)
        .Add(attachment_container_);

    utility_tabs_ = nullptr;
    utility_container_ = nullptr;

    auto* content_split = new BSplitView(B_VERTICAL);
    content_split->AddChild(message_list_);
    content_split->AddChild(preview_container_);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu_bar)
        .Add(toolbar_view_)
        .Add(search_bar_container_)
        .AddGroup(B_VERTICAL, 4)
            .SetInsets(B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING, B_USE_WINDOW_SPACING, B_USE_WINDOW_SPACING)
            .Add(status_view_)
            .Add(content_split)
        .End();

    AddShortcut(B_F7_KEY, B_NO_COMMAND_KEY, new BMessage(kTogglePreviewPaneMessage));
    AddShortcut('F', B_COMMAND_KEY, new BMessage(kCtrlFMessage));
    AddShortcut(B_F3_KEY, B_NO_COMMAND_KEY, new BMessage(kFindAgainMessage));
    AddShortcut('F', B_COMMAND_KEY | B_SHIFT_KEY, new BMessage(kCtrlShiftFMessage));
    AddShortcut('R', B_COMMAND_KEY, new BMessage(kReplyPrimaryShortcutMessage));
    AddShortcut('R', B_COMMAND_KEY | B_SHIFT_KEY, new BMessage(kReplySecondaryShortcutMessage));
    AddShortcut('E', B_COMMAND_KEY, new BMessage(kSendImmediatelyMessage));
    AddShortcut('J', B_COMMAND_KEY, new BMessage(kCtrlJShortcutMessage));
    AddShortcut('J', B_COMMAND_KEY | B_SHIFT_KEY, new BMessage(kCtrlShiftJShortcutMessage));
    AddShortcut('L', B_COMMAND_KEY | B_SHIFT_KEY, new BMessage(kCtrlShiftLShortcutMessage));
    for (int slot = 1; slot <= kSelectedTextUrlSlotCount; ++slot) {
        const uint32 command = SelectedTextUrlCommandForSlot(slot);
        const auto digit = hermes::SelectedTextUrlAcceleratorDigitForSlot(slot);
        if (command != 0 && digit) {
            AddShortcut(static_cast<uint32>('0' + *digit), B_COMMAND_KEY, new BMessage(command));
        }
    }
    AddShortcut(B_SPACE, B_NO_COMMAND_KEY | B_SHIFT_KEY, new BMessage(kToggleStatusMessage));
    AddShortcut(B_UP_ARROW, B_COMMAND_KEY, new BMessage(kPreviousMessageMessage));
    AddShortcut(B_DOWN_ARROW, B_COMMAND_KEY, new BMessage(kNextMessageMessage));
    AddShortcut(B_UP_ARROW, B_NO_COMMAND_KEY | B_CONTROL_KEY, new BMessage(kPreviousMessageMessage));
    AddShortcut(B_DOWN_ARROW, B_NO_COMMAND_KEY | B_CONTROL_KEY, new BMessage(kNextMessageMessage));
    AddShortcut(B_DELETE, B_NO_COMMAND_KEY, new BMessage(kDeleteMessageMessage));
    AddShortcut(B_BACKSPACE, B_NO_COMMAND_KEY, new BMessage(kBackspaceDeleteMessage));
    AddShortcut(B_SPACE, B_NO_COMMAND_KEY, new BMessage(kTocInvokeMessage));
    AddShortcut(B_ENTER, B_NO_COMMAND_KEY, new BMessage(kTocInvokeMessage));
    EnsureCtrlJMappingResolved();
    ApplyFindShortcutMapping();
    RefreshSelectedTextUrlActions();
    PopulateWorkspace();
    PopulateTaskStatus();
    RefreshDynamicMessageMenus();
    RefreshRecentMailboxMenu();
    ApplyGuiPreferences();
    UpdateCommandState();
}

HaikuMainWindow::~HaikuMainWindow() = default;

void HaikuMainWindow::MessageReceived(BMessage* message) {
    const auto command_needs_enablement = [](uint32 what) {
        switch (what) {
            case kCtrlFMessage:
            case kCtrlShiftFMessage:
            case kRefreshMailboxMessage:
            case kResyncMailboxMessage:
            case kOpenMessageMessage:
            case kFindTextMessage:
            case kFindAgainMessage:
            case kSelectedTextUrl1Message:
            case kSelectedTextUrl2Message:
            case kSelectedTextUrl3Message:
            case kSelectedTextUrl4Message:
            case kSelectedTextUrl5Message:
            case kSelectedTextUrl6Message:
            case kSelectedTextUrl7Message:
            case kFindMessagesMessage:
            case kReplyMessage:
            case kReplyAllMessage:
            case kForwardMessage:
            case kRedirectMessage:
            case kSendAgainMessage:
            case kSendImmediatelyMessage:
            case kChangeQueueingMessage:
            case kPreviousMessageMessage:
            case kNextMessageMessage:
            case kDeleteMessageMessage:
            case kUndeleteMessageMessage:
            case kPurgeMailboxMessage:
            case kMarkReadMessage:
            case kMarkUnreadMessage:
            case kToggleStatusMessage:
            case kSetLegacyStatusMessage:
            case kSetLabelMessage:
            case kServerLeaveMessage:
            case kServerFetchMessage:
            case kServerDeleteMessage:
            case kServerFetchDeleteMessage:
            case kMarkJunkMessage:
            case kMarkNotJunkMessage:
            case kRecheckJunkMessage:
            case kFilterMessagesMessage:
            case kMakeFilterMessage:
            case kMoveMessageMessage:
            case kCopyMessageMessage:
            case kMakeNicknameMessage:
            case kPriorityHighestMessage:
            case kPriorityHighMessage:
            case kPriorityNormalMessage:
            case kPriorityLowMessage:
            case kPriorityLowestMessage:
            case kPrintPreviewMessage:
            case kPrintDirectMessage:
            case kFetchFullMessageMessage:
            case kFetchDefaultMessageMessage:
            case kImapRedownloadFullMessage:
            case kImapRedownloadDefaultMessage:
            case kImapClearCachedMessage:
            case kBackspaceDeleteMessage:
            case kTocInvokeMessage:
            case kSpecialWorkOfflineMessage:
            case kSpecialEmptyTrashMessage:
            case kSpecialTrimJunkMessage:
            case kSpecialCompactMailboxesMessage:
            case kSpecialForgetPasswordsMessage:
            case kSpecialChangePasswordMessage:
            case kSpecialOptionsMessage:
            case kWindowSendBehindMessage:
            case kWindowCascadeMessage:
            case kWindowTileHorizontalMessage:
            case kWindowTileVerticalMessage:
            case kWindowArrangeMessage:
            case kWindowCloseAllMessage:
                return true;
            default:
                return false;
        }
    };
    if (command_needs_enablement(message->what) && !IsCommandEnabled(message->what)) {
        return;
    }

    switch (message->what) {
        case kNewComposeMessage:
            shell_host_.OpenComposer(BuildDefaultComposeMessage(shell_host_));
            SetStatusMessage("Opened a new compose window.");
            return;

        case kImportFromEudoraMessage:
            shell_host_.OpenImportWindow();
            SetStatusMessage("Opened the Eudora import window.");
            return;

        case kOpenHelpContentsMessage:
            shell_host_.OpenHelpWindow();
            SetStatusMessage("Opened the help browser.");
            return;

        case kRevealHelpFilesMessage:
            if (shell_host_.RevealHelpFiles()) {
                SetStatusMessage("Revealed the active help files.");
            } else {
                SetStatusMessage("Unable to reveal the help files.");
            }
            return;

        case kMessageSelectedMessage:
            SchedulePreviewRefresh();
            return;

        case kOpenMessageMessage:
            HandleOpenSelectedMessage();
            return;

        case kTocInvokeMessage:
            if (IsFocusWithin(message_list_)) {
                HandleOpenSelectedMessage();
            }
            return;

        case kFindTextMessage:
            ShowFindWindow();
            return;

        case kCtrlFMessage:
            if (mailbox_ui_.search_accel_switch) {
                ShowFindWindow();
            } else {
                HandleFindMessages();
            }
            return;

        case kCtrlShiftFMessage:
            if (mailbox_ui_.search_accel_switch) {
                HandleFindMessages();
            } else {
                ShowFindWindow();
            }
            return;

        case kFindAgainMessage:
            if (HasSharedFindQuery() && HandleTextFind(SharedFindQuery(), true)) {
                SetStatusMessage("Found the next matching mailbox item.");
            } else {
                SetStatusMessage("No further mailbox matches were found.");
            }
            return;

        case kFindMessagesMessage:
            HandleFindMessages();
            return;

        case kSelectedTextUrl1Message:
        case kSelectedTextUrl2Message:
        case kSelectedTextUrl3Message:
        case kSelectedTextUrl4Message:
        case kSelectedTextUrl5Message:
        case kSelectedTextUrl6Message:
        case kSelectedTextUrl7Message:
            HandleSelectedTextUrlCommand(SelectedTextUrlSlotForCommand(message->what));
            return;

        case kCtrlJShortcutMessage:
            EnsureCtrlJMappingResolved();
            if (mailbox_ui_.ctrl_j_mapping == CtrlJMapping::kFilter) {
                HandleFilterSelectedMessages();
            } else {
                HandleJunkAction(MailboxJunkAction::kJunk);
            }
            return;

        case kCtrlShiftJShortcutMessage:
            EnsureCtrlJMappingResolved();
            if (mailbox_ui_.ctrl_j_mapping == CtrlJMapping::kJunk) {
                HandleJunkAction(MailboxJunkAction::kNotJunk);
            }
            return;

        case kCtrlShiftLShortcutMessage:
            EnsureCtrlJMappingResolved();
            if (mailbox_ui_.ctrl_j_mapping == CtrlJMapping::kJunk) {
                HandleFilterSelectedMessages();
            }
            return;

        case kSpecialWorkOfflineMessage:
            HandleWorkOfflineToggle();
            return;

        case kSpecialEmptyTrashMessage:
            HandleEmptyTrash();
            return;

        case kSpecialTrimJunkMessage:
            HandleTrimJunk();
            return;

        case kSpecialCompactMailboxesMessage:
            HandleCompactMailboxes();
            return;

        case kSpecialForgetPasswordsMessage:
            HandleForgetPasswords();
            return;

        case kSpecialChangePasswordMessage:
            HandleChangePasswordDialog();
            return;

        case kSpecialOptionsMessage:
            HandleOptionsDialog();
            return;

        case kWindowSendBehindMessage:
            if (shell_host_.SendActiveWindowToBack()) {
                SetStatusMessage("Sent the active window behind the others.");
            } else {
                SetStatusMessage("Unable to send the active window to the back.");
            }
            return;

        case kWindowCascadeMessage:
            HandleWindowArrangement(HaikuShellHost::WindowArrangeMode::kCascade);
            return;

        case kWindowTileHorizontalMessage:
            HandleWindowArrangement(HaikuShellHost::WindowArrangeMode::kTileHorizontally);
            return;

        case kWindowTileVerticalMessage:
            HandleWindowArrangement(HaikuShellHost::WindowArrangeMode::kTileVertically);
            return;

        case kWindowArrangeMessage:
            HandleWindowArrangement(HaikuShellHost::WindowArrangeMode::kArrange);
            return;

        case kWindowCloseAllMessage:
            HandleCloseAllWindows();
            return;

        case kGroupBySubjectMessage:
            HandleGroupBySubjectToggle();
            return;

        case kAttachmentSelectedMessage:
            return;

        case kSendQueuedMessage: {
            if ((modifiers() & B_SHIFT_KEY) != 0) {
                HandleMailTransferOptions(true);
                return;
            }
            MailTransferRequest request;
            request.send_queued = true;
            const auto summary = shell_host_.ExecuteMailTransfer(request);
            PopulateTaskStatus();
            SetStatusMessage(SummarizeTransportResult(
                summary, "Sent queued mail.", "Send queued reported warnings or errors"));
            return;
        }

        case kCheckMailMessage: {
            if ((modifiers() & B_SHIFT_KEY) != 0) {
                HandleMailTransferOptions(false);
                return;
            }
            MailTransferRequest request;
            request.retrieve_new = true;
            request.send_queued = shell_host_.MailTransfer().send_on_check;
            const auto summary = shell_host_.ExecuteMailTransfer(request);
            PopulateTaskStatus();
            SetStatusMessage(SummarizeTransportResult(
                summary, "Checked mail.", "Mail check reported warnings or errors"));
            return;
        }

        case kSendReceiveMessage: {
            const bool success = shell_host_.SendAndReceive();
            PopulateTaskStatus();
            SetStatusMessage(success ? "Send and receive complete." :
                                       "Send and receive reported warnings or errors.");
            return;
        }

        case kBackspaceDeleteMessage:
            if (shell_host_.ShellBehavior().backspace_delete && IsFocusWithin(message_list_)) {
                BMessage delete_message(kDeleteMessageMessage);
                PostMessage(&delete_message);
            }
            return;

        case kPreviousMessageMessage:
            HandlePreviousMessage();
            return;

        case kNextMessageMessage:
            HandleNextMessage();
            return;

        case kStopTasksMessage:
            shell_host_.StopActiveTasks();
            PopulateTaskStatus();
            SetStatusMessage("Stopped active background tasks.");
            return;

        case kRefreshMailboxMessage:
            if (!current_mailbox_id_.empty()) {
                const bool success = shell_host_.RefreshMailbox(current_mailbox_id_);
                PopulateTaskStatus();
                SetStatusMessage(success ? "Mailbox refresh queued." :
                                           "Mailbox refresh reported warnings or errors.");
            }
            return;

        case kResyncMailboxMessage:
            if (!current_mailbox_id_.empty()) {
                const bool success = shell_host_.ResyncMailbox(current_mailbox_id_);
                PopulateTaskStatus();
                SetStatusMessage(success ? "Mailbox resync queued." :
                                           "Mailbox resync reported warnings or errors.");
            }
            return;

        case kCreateMailboxMessage:
            HandleCreateMailbox();
            return;

        case kRenameMailboxMessage:
            HandleRenameMailbox();
            return;

        case kDeleteMailboxMessage:
            HandleDeleteMailbox();
            return;

        case kCreateMailboxConfirmed: {
            const char* account_id = nullptr;
            const char* name = nullptr;
            if (message->FindString("account_id", &account_id) == B_OK &&
                message->FindString("name", &name) == B_OK && account_id != nullptr && name != nullptr) {
                const bool queued = shell_host_.CreateRemoteMailbox(account_id, name);
                PopulateTaskStatus();
                SetStatusMessage(
                    queued ? "Remote mailbox creation queued." : "Unable to queue remote mailbox creation.");
                if (!queued) {
                    SelectUtilityTab(1);
                }
            }
            return;
        }

        case kRenameMailboxConfirmed: {
            const char* mailbox_id = nullptr;
            const char* name = nullptr;
            if (message->FindString("mailbox_id", &mailbox_id) == B_OK &&
                message->FindString("name", &name) == B_OK && mailbox_id != nullptr && name != nullptr) {
                const bool queued = shell_host_.RenameRemoteMailbox(mailbox_id, name);
                PopulateTaskStatus();
                SetStatusMessage(
                    queued ? "Remote mailbox rename queued." : "Unable to queue remote mailbox rename.");
                if (!queued) {
                    SelectUtilityTab(1);
                }
            }
            return;
        }

        case kDeleteMessageMessage: {
            bool queued = false;
            const auto selected_ids = SelectedMessageIds();
            if (!selected_ids.empty()) {
                std::vector<std::string> warnings;
                for (const auto& message_id : selected_ids) {
                    if (const auto detail = shell_host_.WorkspaceMessageDetail(message_id)) {
                        if (detail->unread) {
                            warnings.push_back("At least one selected message is still unread.");
                            break;
                        }
                    }
                }
                for (const auto& message_id : selected_ids) {
                    if (const auto record = shell_host_.Messages().GetMessage(current_mailbox_id_, message_id)) {
                        if (record->delivery_state == MessageDeliveryState::kQueued) {
                            warnings.push_back("At least one selected message is queued to send.");
                            break;
                        }
                        if (record->delivery_state == MessageDeliveryState::kDraft) {
                            warnings.push_back("At least one selected message is still a draft.");
                            break;
                        }
                    }
                }
                if (!warnings.empty()) {
                    const std::string prompt = "Delete " + std::to_string(selected_ids.size()) +
                                               " selected message(s)?\n\n" + JoinLines(warnings);
                    if (BAlert("delete-message-warning", prompt.c_str(), "Cancel", "Delete")->Go() != 1) {
                        SetStatusMessage("Message deletion cancelled.");
                        return;
                    }
                }
                for (const auto& message_id : selected_ids) {
                    queued = shell_host_.DeleteMessage(current_mailbox_id_, message_id) || queued;
                }
            }
            PopulateTaskStatus();
            SetStatusMessage(queued ? "Message delete queued." : "Unable to queue message deletion.");
            return;
        }

        case kUndeleteMessageMessage: {
            bool queued = false;
            for (const auto& message_id : SelectedMessageIds()) {
                queued = shell_host_.UndeleteMessage(current_mailbox_id_, message_id) || queued;
            }
            PopulateTaskStatus();
            SetStatusMessage(queued ? "Message undeletion queued." :
                                      "Unable to queue message undeletion.");
            return;
        }

        case kPurgeMailboxMessage: {
            const bool queued = !current_mailbox_id_.empty() && shell_host_.PurgeMailbox(current_mailbox_id_);
            PopulateTaskStatus();
            SetStatusMessage(queued ? "IMAP purge/expunge queued." :
                                      "Unable to queue IMAP purge/expunge.");
            return;
        }

        case kMoveMessageMessage:
            HandleMoveOrCopySelectedMessage(false);
            return;

        case kCopyMessageMessage:
            HandleMoveOrCopySelectedMessage(true);
            return;

        case kOpenRecentMailboxMessage: {
            const char* mailbox_id = nullptr;
            if (message->FindString("mailbox_id", &mailbox_id) == B_OK && mailbox_id != nullptr) {
                const bool opened = shell_host_.OpenMailbox(mailbox_id);
                SetStatusMessage(opened ? "Opened the selected recent mailbox."
                                        : "Unable to open the selected recent mailbox.");
            }
            return;
        }

        case kPerformMoveMessage: {
            const char* destination_mailbox_id = nullptr;
            if (message->FindString("destination_mailbox_id", &destination_mailbox_id) == B_OK &&
                destination_mailbox_id != nullptr) {
                bool queued = false;
                for (const auto& message_id : SelectedMessageIds()) {
                    queued = shell_host_.MoveMessage(current_mailbox_id_, message_id, destination_mailbox_id) || queued;
                }
                PopulateTaskStatus();
                SetStatusMessage(queued ? "Message move queued." : "Unable to queue message move.");
            }
            return;
        }

        case kPerformCopyMessage: {
            const char* destination_mailbox_id = nullptr;
            if (message->FindString("destination_mailbox_id", &destination_mailbox_id) == B_OK &&
                destination_mailbox_id != nullptr) {
                bool queued = false;
                for (const auto& message_id : SelectedMessageIds()) {
                    queued = shell_host_.CopyMessage(current_mailbox_id_, message_id, destination_mailbox_id) || queued;
                }
                PopulateTaskStatus();
                SetStatusMessage(queued ? "Message copy queued." : "Unable to queue message copy.");
            }
            return;
        }

        case kFetchFullMessageMessage:
            HandleFetchSelectedMessage(false);
            return;

        case kReplyPrimaryShortcutMessage:
            HandleReplyShortcut(false);
            return;

        case kReplySecondaryShortcutMessage:
            HandleReplyShortcut(true);
            return;

        case kReplyMessage:
            HandleComposeResponse(static_cast<int>(HaikuShellHost::MessageResponseKind::kReply));
            return;

        case kReplyAllMessage:
            HandleComposeResponse(static_cast<int>(HaikuShellHost::MessageResponseKind::kReplyAll));
            return;

        case kForwardMessage:
            HandleComposeResponse(static_cast<int>(HaikuShellHost::MessageResponseKind::kForward));
            return;

        case kRedirectMessage:
            HandleComposeResponse(static_cast<int>(HaikuShellHost::MessageResponseKind::kRedirect));
            return;

        case kSendAgainMessage:
            HandleComposeResponse(static_cast<int>(HaikuShellHost::MessageResponseKind::kSendAgain));
            return;

        case kSendImmediatelyMessage:
            HandleSendImmediately();
            return;

        case kChangeQueueingMessage:
            HandleChangeQueueingPrompt();
            return;

        case kChangeQueueingConfirmedMessage: {
            const char* value = nullptr;
            if (message->FindString("name", &value) == B_OK && value != nullptr) {
                HandleChangeQueueing(std::max(0, std::atoi(value)));
            }
            return;
        }

        case kToggleStatusMessage:
            HandleToggleSelectedStatus();
            return;

        case kMarkReadMessage:
            HandleMarkSelectedMessageUnread(false);
            return;

        case kMarkUnreadMessage:
            HandleMarkSelectedMessageUnread(true);
            return;

        case kSetLegacyStatusMessage: {
            int32 status = 0;
            if (message->FindInt32("legacy_status", &status) == B_OK) {
                HandleSetSelectedLegacyStatus(LegacyStatusFromInt(status));
            } else {
                const char* status_value = nullptr;
                if (message->FindString("legacy_status", &status_value) == B_OK && status_value != nullptr) {
                    HandleSetSelectedLegacyStatus(LegacyStatusFromInt(std::atoi(status_value)));
                }
            }
            return;
        }

        case kSetLabelMessage: {
            int32 label_index = 0;
            if (message->FindInt32("label_index", &label_index) == B_OK) {
                HandleSetSelectedLabel(label_index);
            }
            return;
        }

        case kMarkJunkMessage:
            HandleJunkAction(MailboxJunkAction::kJunk);
            return;

        case kMarkNotJunkMessage:
            HandleJunkAction(MailboxJunkAction::kNotJunk);
            return;

        case kRecheckJunkMessage:
            HandleJunkAction(MailboxJunkAction::kRecheck);
            return;

        case kServerLeaveMessage:
            HandleSetSelectedPopServerStatus(PopServerStatus::kLeave);
            return;

        case kServerFetchMessage:
            HandleSetSelectedPopServerStatus(PopServerStatus::kFetch);
            return;

        case kServerDeleteMessage:
            HandleSetSelectedPopServerStatus(PopServerStatus::kDelete);
            return;

        case kServerFetchDeleteMessage:
            HandleSetSelectedPopServerStatus(PopServerStatus::kFetchDelete);
            return;

        case kFilterMessagesMessage:
            HandleFilterSelectedMessages();
            return;

        case kMakeFilterMessage:
            HandleMakeFilter();
            return;

        case kPriorityHighestMessage:
            HandleSetSelectedPriority(static_cast<int>(ComposePriority::kHighest));
            return;

        case kPriorityHighMessage:
            HandleSetSelectedPriority(static_cast<int>(ComposePriority::kHigh));
            return;

        case kPriorityNormalMessage:
            HandleSetSelectedPriority(static_cast<int>(ComposePriority::kNormal));
            return;

        case kPriorityLowMessage:
            HandleSetSelectedPriority(static_cast<int>(ComposePriority::kLow));
            return;

        case kPriorityLowestMessage:
            HandleSetSelectedPriority(static_cast<int>(ComposePriority::kLowest));
            return;

        case kMakeNicknameMessage:
            HandleMakeNickname();
            return;

        case kDynamicNewStationeryMessage: {
            const char* stationery_name = nullptr;
            if (message->FindString("stationery_name", &stationery_name) == B_OK && stationery_name != nullptr) {
                const auto stationery = shell_host_.Stationery().Find(stationery_name);
                if (!stationery) {
                    SetStatusMessage("Unable to locate the selected stationery.");
                    return;
                }
                ComposeMessage compose = BuildDefaultComposeMessage(shell_host_);
                compose.headers = stationery->headers;
                compose.body = stationery->body;
                compose.stationery_name = stationery->name;
                compose.signature_name =
                    stationery->signature_name.empty() ? compose.signature_name : stationery->signature_name;
                if (!stationery->persona.empty()) {
                    compose.headers.from_persona = stationery->persona;
                }
                shell_host_.OpenComposer(compose);
            }
            return;
        }

        case kDynamicReplyStationeryMessage: {
            const char* stationery_name = nullptr;
            if (message->FindString("stationery_name", &stationery_name) == B_OK && stationery_name != nullptr) {
                HandleComposeResponse(static_cast<int>(HaikuShellHost::MessageResponseKind::kReply), stationery_name);
            }
            return;
        }

        case kDynamicReplyAllStationeryMessage: {
            const char* stationery_name = nullptr;
            if (message->FindString("stationery_name", &stationery_name) == B_OK && stationery_name != nullptr) {
                HandleComposeResponse(static_cast<int>(HaikuShellHost::MessageResponseKind::kReplyAll), stationery_name);
            }
            return;
        }

        case kDynamicChangePersonaMessage: {
            const char* persona_id = nullptr;
            if (message->FindString("persona_id", &persona_id) != B_OK || persona_id == nullptr) {
                return;
            }
            if (current_mailbox_id_ != "out") {
                SetStatusMessage("Change Personality is only available for Out mailbox messages.");
                return;
            }
            const auto account = shell_host_.Accounts().FindById(persona_id);
            if (!account) {
                SetStatusMessage("Unable to locate the selected personality.");
                return;
            }
            const auto selected_ids = SelectedMessageIds();
            if (selected_ids.empty()) {
                return;
            }
            std::string error_message;
            bool updated_any = false;
            for (const auto& message_id : selected_ids) {
                auto record = shell_host_.Messages().GetMessage(current_mailbox_id_, message_id);
                if (!record) {
                    continue;
                }
                record->account_id = account->id;
                record->sender = account->display_name.empty() ? account->email_address : account->display_name;
                record->updated_at = std::time(nullptr);
                if (!shell_host_.Messages().SaveMessage(*record, &error_message)) {
                    SetStatusMessage(error_message.empty() ? "Unable to change personality for the selected messages."
                                                           : error_message);
                    return;
                }
                updated_any = true;
            }
            if (updated_any) {
                shell_host_.ReloadWorkspace();
                SetStatusMessage("Changed personality for the selected messages.");
            }
            return;
        }

        case kDynamicNewRecipientMessage:
        case kDynamicForwardRecipientMessage:
        case kDynamicRedirectRecipientMessage: {
            const char* nickname_name = nullptr;
            if (message->FindString("nickname_name", &nickname_name) == B_OK && nickname_name != nullptr) {
                HandleDynamicRecipientCompose(message->what, nickname_name);
            }
            return;
        }

        case kPrintPreviewMessage:
            HandlePrintSelectedMessage(true);
            return;

        case kPrintDirectMessage:
            HandlePrintSelectedMessage(false);
            return;

        case kOpenAttachmentMessage:
            HandleOpenSelectedAttachment();
            return;

        case kSaveAttachmentMessage:
            HandleSaveSelectedAttachment();
            return;

        case kSaveAllAttachmentsMessage:
            HandleSaveAllAttachments();
            return;

        case kFetchAttachmentMessage:
            HandleFetchSelectedAttachment();
            return;

        case kFetchDefaultMessageMessage:
            HandleFetchSelectedMessage(true);
            return;

        case kImapRedownloadFullMessage:
            HandleRedownloadSelectedMessages(true);
            return;

        case kImapRedownloadDefaultMessage:
            HandleRedownloadSelectedMessages(false);
            return;

        case kImapClearCachedMessage:
            HandleClearCachedSelectedMessages();
            return;

        case kRetryTaskMessage: {
            shell_host_.OpenToolWindow("task-status");
            SetStatusMessage("Use the Tasks window to retry the selected action.");
            return;
        }

        case kCancelTaskMessage: {
            shell_host_.OpenToolWindow("task-status");
            SetStatusMessage("Use the Tasks window to cancel the selected action.");
            return;
        }

        case kTogglePreviewPaneMessage:
            TogglePreviewPane();
            return;

        case kToggleToolbarMessage:
            ToggleToolbar();
            return;

        case kToggleSearchBarMessage:
            ToggleSearchBar();
            return;

        case kToggleUtilityPaneMessage:
            ToggleUtilityPane();
            return;

        case kSearchBarScopeChangedMessage: {
            int32 scope_mode = static_cast<int32>(SearchBarMode::kSearchWeb);
            if (message->FindInt32("scope_mode", &scope_mode) == B_OK) {
                search_bar_mode_ = ParseSearchBarMode(scope_mode).value_or(SearchBarMode::kSearchWeb);
                UpdateSearchBarState();
                PersistGuiPreferences();
            }
            return;
        }

        case kSearchBarRecentChosenMessage: {
            int32 recent_index = -1;
            if (message->FindInt32("recent_index", &recent_index) == B_OK) {
                HandleSearchBarRecentSelection(recent_index);
            }
            return;
        }

        case kSearchBarActionChosenMessage: {
            int32 scope_mode = static_cast<int32>(SearchBarMode::kSearchWeb);
            if (message->FindInt32("scope_mode", &scope_mode) == B_OK) {
                search_bar_mode_ = ParseSearchBarMode(scope_mode).value_or(SearchBarMode::kSearchWeb);
                const std::string query = SearchBarQueryText();
                UpdateSearchBarState();
                PersistGuiPreferences();
                if (!query.empty()) {
                    ExecuteSearchBarQuery();
                } else if (search_bar_query_control_ != nullptr &&
                           search_bar_query_control_->TextView() != nullptr) {
                    search_bar_query_control_->TextView()->MakeFocus(true);
                    const int32 length = static_cast<int32>(std::strlen(search_bar_query_control_->Text()));
                    search_bar_query_control_->TextView()->Select(0, length);
                }
            }
            return;
        }

        case kSearchBarFocusChangedMessage: {
            bool focused = false;
            if (message->FindBool("focused", &focused) == B_OK) {
                HandleSearchBarFocusChanged(focused);
            }
            return;
        }

        case kSearchBarQueryModifiedMessage:
            if (updating_search_bar_query_) {
                return;
            }
            if (search_bar_prompt_visible_ && SearchBarQueryText() != SearchBarPromptText()) {
                search_bar_prompt_visible_ = false;
            }
            UpdateSearchBarState();
            return;

        case kSearchBarExecuteMessage:
            ExecuteSearchBarQuery();
            return;

        case kSelectTaskStatusTabMessage:
            gui_preferences_.tasks_wazoo.open = true;
            UpdateViewMenuMarks();
            shell_host_.OpenToolWindow("task-status");
            return;

        case kSelectTaskErrorsTabMessage:
            gui_preferences_.tasks_wazoo.open = true;
            UpdateViewMenuMarks();
            shell_host_.OpenToolWindow("task-errors");
            return;

        case kCustomizeToolbarMessage: {
            if (toolbar_customization_window_ != nullptr) {
                if (toolbar_customization_window_->IsHidden()) {
                    toolbar_customization_window_->Show();
                } else {
                    toolbar_customization_window_->Activate(true);
                }
                return;
            }
            const auto actions = MainToolbarActionSpecs(shell_host_);
            const auto allowed_entries = ToolbarAllowedEntries(actions);
            auto configuration = ParseToolbarConfiguration(gui_preferences_.main_toolbar_layout,
                                                           allowed_entries,
                                                           MainToolbarDefaultEntries());
            toolbar_customization_window_ = std::make_unique<HaikuToolbarCustomizationWindow>(
                "Customize Main Toolbar",
                actions,
                MainToolbarDefaultEntries(),
                std::move(configuration),
                gui_preferences_.show_toolbar_tips,
                gui_preferences_.show_toolbar_large_buttons,
                [this](const hermes::ToolbarConfiguration& updated,
                       bool show_tool_tips,
                       bool large_buttons) {
                    gui_preferences_.main_toolbar_layout = SerializeToolbarConfiguration(updated);
                    gui_preferences_.show_toolbar_tips = show_tool_tips;
                    gui_preferences_.show_toolbar_large_buttons = large_buttons;
                    RebuildToolbar();
                    PersistGuiPreferences();
                });
            toolbar_customization_window_->Show();
            return;
        }

        case kOpenToolWindowMessage: {
            const char* tool_id = nullptr;
            if (message->FindString("tool_id", &tool_id) == B_OK && tool_id != nullptr) {
                if (std::string_view(tool_id) == "task-status" || std::string_view(tool_id) == "task-errors") {
                    gui_preferences_.tasks_wazoo.open = true;
                    UpdateViewMenuMarks();
                }
                if (std::string_view(tool_id) == "directory-services") {
                    const std::string selected_text = SelectedFocusedText();
                    if (!selected_text.empty()) {
                        shell_host_.QueuePendingDirectoryQuery(selected_text);
                    }
                }
                shell_host_.OpenToolWindow(tool_id);
            }
            return;
        }

        case kToolbarOpenMailboxMessage: {
            const char* mailbox_id = nullptr;
            if (message->FindString("mailbox_id", &mailbox_id) == B_OK && mailbox_id != nullptr) {
                shell_host_.OpenMailbox(mailbox_id);
                shell_host_.ShowMainWindow();
            }
            return;
        }

        case kToolbarComposeNicknameMessage: {
            const char* nickname = nullptr;
            if (message->FindString("nickname", &nickname) != B_OK || nickname == nullptr) {
                return;
            }
            const auto entry = shell_host_.Nicknames().FindNickname(nickname);
            if (!entry) {
                SetStatusMessage("Unable to resolve the selected nickname.");
                return;
            }
            ComposeMessage compose = BuildDefaultComposeMessage(shell_host_);
            compose.headers.to = JoinCommaList(entry->addresses);
            shell_host_.OpenComposer(compose);
            return;
        }

        case kToolbarComposeStationeryMessage: {
            const char* name = nullptr;
            if (message->FindString("name", &name) != B_OK || name == nullptr) {
                return;
            }
            const auto stationery = shell_host_.Stationery().Find(name);
            if (!stationery) {
                SetStatusMessage("Unable to locate the selected stationery.");
                return;
            }
            ComposeMessage compose = BuildDefaultComposeMessage(shell_host_);
            compose.headers = stationery->headers;
            compose.body = stationery->body;
            compose.stationery_name = stationery->name;
            compose.signature_name = stationery->signature_name.empty()
                                         ? compose.signature_name
                                         : stationery->signature_name;
            if (!stationery->persona.empty()) {
                compose.headers.from_persona = stationery->persona;
            }
            shell_host_.OpenComposer(compose);
            return;
        }

        case kToolbarComposePersonaMessage: {
            const char* persona_id = nullptr;
            if (message->FindString("persona_id", &persona_id) != B_OK || persona_id == nullptr) {
                return;
            }
            ComposeMessage compose = BuildDefaultComposeMessage(shell_host_);
            compose.headers.from_persona = persona_id;
            shell_host_.OpenComposer(compose);
            return;
        }

        case kToolbarRevealPluginMessage: {
            const char* path = nullptr;
            if (message->FindString("path", &path) != B_OK || path == nullptr) {
                return;
            }
            const std::filesystem::path plugin_path(path);
            if (!LaunchPath(plugin_path.parent_path().empty() ? plugin_path : plugin_path.parent_path())) {
                SetStatusMessage("Unable to reveal the selected plugin.");
            }
            return;
        }

        case kPreviewReadTickMessage:
            MarkSelectedMessageReadFromPreview();
            return;

        case kPreviewRefreshTickMessage:
            PopulatePreviewNow();
            return;

        case kAutoFetchPreviewMessage: {
            const char* mailbox_id = nullptr;
            const char* message_id = nullptr;
            if (message->FindString("mailbox_id", &mailbox_id) != B_OK || mailbox_id == nullptr ||
                message->FindString("message_id", &message_id) != B_OK || message_id == nullptr ||
                current_mailbox_id_ != mailbox_id || current_message_id_ != message_id) {
                return;
            }
            const bool queued = shell_host_.FetchFullMessage(mailbox_id, message_id) && shell_host_.CheckMail();
            PopulateTaskStatus();
            if (queued) {
                SetStatusMessage("Queued full IMAP preview fetch.");
            } else {
                preview_render_notice_ = "Unable to fetch the full IMAP preview; showing cached content";
                if (const auto detail = shell_host_.WorkspaceMessageDetail(message_id)) {
                    PopulatePreviewBody(*detail);
                    PopulatePreviewHeader(*detail);
                    PopulateAttachments(&*detail);
                }
                SetStatusMessage("Unable to fetch the full IMAP preview.");
            }
            return;
        }

        case kFindConfirmedMessage: {
            const char* query = nullptr;
            if (message->FindString("query", &query) == B_OK && query != nullptr) {
                SetSharedFindQuery(query);
                if (HandleTextFind(query, false)) {
                    SetStatusMessage("Found a matching mailbox item.");
                } else {
                    SetStatusMessage("No mailbox matches were found.");
                }
            }
            return;
        }

        case kFindClosedMessage:
            RestoreFindFocus();
            return;

        case kMailTransferOptionsAcceptedMessage: {
            MailTransferSettings transfer_settings = shell_host_.MailTransfer();
            int32 persona_mode = 1;
            if (message->FindInt32("transfer_persona_options", &persona_mode) == B_OK) {
                transfer_settings.transfer_persona_options =
                    persona_mode == 0 ? TransferPersonaOptionsMode::kSpecifiedOptions
                                      : TransferPersonaOptionsMode::kNormalOptions;
                ApplyMailTransferSettingsToSettings(transfer_settings, shell_host_.Settings());
                std::string ignored;
                shell_host_.PersistSettings(&ignored);
            }

            MailTransferRequest request;
            request.ignore_check_mail_by_default = true;
            bool bool_value = false;
            if (message->FindBool("send_queued", &bool_value) == B_OK) {
                request.send_queued = bool_value;
            }
            if (message->FindBool("retrieve_new", &bool_value) == B_OK) {
                request.retrieve_new = bool_value;
            }
            if (message->FindBool("delete_marked", &bool_value) == B_OK) {
                request.delete_marked = bool_value;
            }
            if (message->FindBool("retrieve_marked", &bool_value) == B_OK) {
                request.retrieve_marked = bool_value;
            }
            if (message->FindBool("delete_retrieved", &bool_value) == B_OK) {
                request.delete_retrieved = bool_value;
            }
            if (message->FindBool("delete_all", &bool_value) == B_OK) {
                request.delete_all = bool_value;
            }
            if (message->FindBool("fetch_headers", &bool_value) == B_OK) {
                request.fetch_headers = bool_value;
            }
            int32 mode_value = 1;
            (void)message->FindInt32("transfer_persona_options", &mode_value);
            if (mode_value == 1) {
                bool sending_only = false;
                (void)message->FindBool("sending_only", &sending_only);
                request.send_queued = sending_only ? true : transfer_settings.send_on_check;
                request.retrieve_new = !sending_only;
                request.delete_marked = false;
                request.retrieve_marked = false;
                request.delete_retrieved = !transfer_settings.leave_mail_on_server && !sending_only;
                request.delete_all = false;
                request.fetch_headers = false;
            }
            const char* account_id = nullptr;
            for (int32 index = 0; message->FindString("account_id", index, &account_id) == B_OK; ++index) {
                if (account_id != nullptr && account_id[0] != '\0') {
                    request.selected_account_ids.push_back(account_id);
                }
            }
            const auto summary = shell_host_.ExecuteMailTransfer(request);
            PopulateTaskStatus();
            SetStatusMessage(SummarizeTransportResult(
                summary, "Executed mail transfer options.", "Mail transfer reported warnings or errors"));
            return;
        }

        case kPasswordChangedMessage: {
            const char* account_id = nullptr;
            const char* incoming_password = nullptr;
            const char* outgoing_password = nullptr;
            if (message->FindString("account_id", &account_id) != B_OK || account_id == nullptr ||
                message->FindString("incoming_password", &incoming_password) != B_OK || incoming_password == nullptr ||
                message->FindString("outgoing_password", &outgoing_password) != B_OK || outgoing_password == nullptr) {
                return;
            }
            std::string error_message;
            if (shell_host_.UpdateAccountPasswords(account_id, incoming_password, outgoing_password, &error_message)) {
                SetStatusMessage("Updated stored account passwords.");
            } else {
                SetStatusMessage(error_message.empty() ? "Unable to update account passwords." : error_message);
            }
            return;
        }

        case kOptionsAcceptedMessage: {
            ShellBehaviorSettings shell_behavior = shell_host_.ShellBehavior();
            MailTransferSettings mail_transfer = shell_host_.MailTransfer();
            MailboxUiSettings mailbox_ui = shell_host_.MailboxUi();
            GuiPreferences gui_preferences = gui_preferences_;
            bool bool_value = false;
            int32 int_value = 0;

            if (message->FindBool("offline", &bool_value) == B_OK) {
                shell_behavior.offline = bool_value;
            }
            if (message->FindBool("control_arrows", &bool_value) == B_OK) {
                shell_behavior.control_arrows = bool_value;
            }
            if (message->FindBool("alt_arrows", &bool_value) == B_OK) {
                shell_behavior.alt_arrows = bool_value;
            }
            if (message->FindBool("backspace_delete", &bool_value) == B_OK) {
                shell_behavior.backspace_delete = bool_value;
            }
            if (message->FindBool("reply_ctrl_r_to_all", &bool_value) == B_OK) {
                shell_behavior.reply_ctrl_r_to_all = bool_value;
            }
            if (message->FindBool("search_accel_switch", &bool_value) == B_OK) {
                mailbox_ui.search_accel_switch = bool_value;
            }
            if (message->FindBool("mailbox_show_status", &bool_value) == B_OK) {
                mailbox_ui.mailbox_show_status = bool_value;
            }
            if (message->FindBool("mailbox_show_junk", &bool_value) == B_OK) {
                mailbox_ui.mailbox_show_junk = bool_value;
            }
            if (message->FindBool("mailbox_show_label", &bool_value) == B_OK) {
                mailbox_ui.mailbox_show_label = bool_value;
            }
            if (message->FindBool("mailbox_show_server_status", &bool_value) == B_OK) {
                mailbox_ui.mailbox_show_server_status = bool_value;
            }
            if (message->FindBool("show_mailbox_lines", &bool_value) == B_OK) {
                mailbox_ui.show_mailbox_lines = bool_value;
            }
            if (message->FindBool("black_toc_lines", &bool_value) == B_OK) {
                mailbox_ui.black_toc_lines = bool_value;
            }
            if (message->FindBool("whole_summary_label_color", &bool_value) == B_OK) {
                mailbox_ui.whole_summary_label_color = bool_value;
            }
            if (message->FindBool("comp_summary_italic", &bool_value) == B_OK) {
                mailbox_ui.comp_summary_italic = bool_value;
            }
            if (message->FindBool("mark_previewed_read", &bool_value) == B_OK) {
                gui_preferences.mark_previewed_read = bool_value;
            }
            if (message->FindBool("show_toolbar_tips", &bool_value) == B_OK) {
                gui_preferences.show_toolbar_tips = bool_value;
            }
            if (message->FindBool("show_toolbar_large_buttons", &bool_value) == B_OK) {
                gui_preferences.show_toolbar_large_buttons = bool_value;
            }
            if (message->FindBool("immediate_send", &bool_value) == B_OK) {
                mail_transfer.immediate_send = bool_value;
            }
            if (message->FindBool("send_on_check", &bool_value) == B_OK) {
                mail_transfer.send_on_check = bool_value;
            }
            if (message->FindBool("leave_mail_on_server", &bool_value) == B_OK) {
                mail_transfer.leave_mail_on_server = bool_value;
            }
            if (message->FindBool("always_enable_junk", &bool_value) == B_OK) {
                mailbox_ui.always_enable_junk = bool_value;
            }
            if (message->FindBool("delete_fetched_junk", &bool_value) == B_OK) {
                mailbox_ui.delete_fetched_junk = bool_value;
            }
            if (message->FindBool("multiple_replies_for_multiple_selections", &bool_value) == B_OK) {
                mailbox_ui.multiple_replies_for_multiple_selections = bool_value;
            }
            if (message->FindInt32("preview_read_seconds", &int_value) == B_OK) {
                gui_preferences.preview_read_seconds = std::max(0, static_cast<int>(int_value));
            }
            if (message->FindInt32("multiple_reply_warn_threshold", &int_value) == B_OK) {
                mailbox_ui.multiple_reply_warn_threshold = std::max(1, static_cast<int>(int_value));
            }

            ApplyShellBehaviorSettingsToSettings(shell_behavior, shell_host_.Settings());
            ApplyMailTransferSettingsToSettings(mail_transfer, shell_host_.Settings());
            ApplyMailboxUiSettingsToSettings(mailbox_ui, shell_host_.Settings());
            gui_preferences_ = gui_preferences;
            ApplyGuiPreferencesToSettings(gui_preferences_, shell_host_.Settings());
            std::string error_message;
            if (shell_host_.PersistSettings(&error_message)) {
                shell_host_.ReloadWorkspace();
                SetStatusMessage("Updated shell options.");
            } else {
                SetStatusMessage(error_message.empty() ? "Unable to persist updated options." : error_message);
            }
            return;
        }

        default:
            BWindow::MessageReceived(message);
            return;
    }
}

void HaikuMainWindow::MenusBeginning() {
    BWindow::MenusBeginning();
    RefreshRecentMailboxMenu();
    UpdateCommandState();
}

bool HaikuMainWindow::QuitRequested() {
    if ((modifiers() & B_SHIFT_KEY) != 0) {
        std::string error_message;
        if (shell_host_.SaveOpenWindowLayout(&error_message)) {
            SetStatusMessage("Saved the current window layout.");
        } else {
            SetStatusMessage(error_message.empty() ? "Unable to save the current window layout."
                                                   : error_message);
        }
        return false;
    }
    PersistGuiPreferences();
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}

void HaikuMainWindow::PopulateWorkspace() {
    mailbox_ids_.clear();
    attachment_indices_.clear();

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

    for (const auto& mailbox : mailboxes) {
        mailbox_ids_.push_back(mailbox.id);
    }

    if (!shell_host_.ActiveMailboxId().empty()) {
        current_mailbox_id_ = shell_host_.ActiveMailboxId();
    }
    if (current_mailbox_id_.empty() && !mailbox_ids_.empty()) {
        current_mailbox_id_ = mailbox_ids_.front();
    }
    if (current_mailbox_id_.empty()) {
        current_mailbox_id_ = "inbox";
    }

    int32 selected_mailbox_index = -1;
    for (std::size_t index = 0; index < mailbox_ids_.size(); ++index) {
        if (mailbox_ids_[index] == current_mailbox_id_) {
            selected_mailbox_index = static_cast<int32>(index);
            break;
        }
    }
    if (selected_mailbox_index < 0 && !mailbox_ids_.empty()) {
        selected_mailbox_index = 0;
        current_mailbox_id_ = mailbox_ids_.front();
    }
    PopulateMessagesForCurrentMailbox();
    PopulatePreview();
}

void HaikuMainWindow::PopulateMessagesForCurrentMailbox() {
    message_list_->Clear();
    UpdateTocColumnVisibility();

    auto messages = shell_host_.Workspace().MessagesForMailbox(current_mailbox_id_);
    std::stable_sort(messages.begin(), messages.end(), [this](const MessageSummary& left, const MessageSummary& right) {
        if (gui_preferences_.toc_sort.group_by_subject) {
            const int grouped_subject = CompareMailboxSubjects(left.subject, right.subject);
            if (grouped_subject != 0) {
                return grouped_subject < 0;
            }
        }
        if (gui_preferences_.toc_sort.primary_field >= 0) {
            const int primary =
                ApplyDirection(CompareTocField(left, right, gui_preferences_.toc_sort.primary_field),
                               gui_preferences_.toc_sort.primary_descending);
            if (primary != 0) {
                return primary < 0;
            }
        }
        if (gui_preferences_.toc_sort.secondary_field >= 0) {
            const int secondary =
                ApplyDirection(CompareTocField(left, right, gui_preferences_.toc_sort.secondary_field),
                               gui_preferences_.toc_sort.secondary_descending);
            if (secondary != 0) {
                return secondary < 0;
            }
        }
        if (left.unread != right.unread) {
            return left.unread && !right.unread;
        }
        if (left.timestamp != right.timestamp) {
            return left.timestamp > right.timestamp;
        }
        const int subject_compare = CompareMailboxSubjects(left.subject, right.subject);
        if (subject_compare != 0) {
            return subject_compare < 0;
        }
        return ToLower(left.id) < ToLower(right.id);
    });
    for (const auto& message : messages) {
        message_list_->AddRow(new TocMessageRow(message, mailbox_ui_, BuildTocRowStyle(message, mailbox_ui_, current_mailbox_id_)));
    }

    int32 selected_message_index = -1;
    for (int32 index = 0; const auto* row = dynamic_cast<const TocMessageRow*>(message_list_->RowAt(index)); ++index) {
        if (!current_message_id_.empty() && row->message_id == current_message_id_) {
            selected_message_index = index;
            break;
        }
    }
    if (selected_message_index < 0 && message_list_->CountRows() > 0) {
        selected_message_index = 0;
    }
    if (selected_message_index >= 0) {
        if (auto* row = dynamic_cast<TocMessageRow*>(message_list_->RowAt(selected_message_index))) {
            message_list_->DeselectAll();
            message_list_->SetFocusRow(row, true);
            current_message_id_ = row->message_id;
        }
    } else {
        current_message_id_.clear();
    }
}

void HaikuMainWindow::PopulatePreviewHeader(const hermes::MessageDetail& detail) {
    preview_subject_->SetText(("Subject: " + detail.subject).c_str());
    preview_from_->SetText(("From: " + detail.sender).c_str());
    preview_to_->SetText(("To: " + detail.recipients).c_str());

    std::string date_label = "Date: ";
    if (const auto record = shell_host_.Messages().GetMessage(detail.mailbox_id, detail.id)) {
        const std::string timestamp =
            !FormatTimestamp(record->updated_at).empty() ? FormatTimestamp(record->updated_at)
                                                         : FormatTimestamp(record->created_at);
        if (!timestamp.empty()) {
            date_label += timestamp;
        } else {
            date_label += "Unavailable";
        }
    } else if (detail.mailbox_id == "drafts") {
        date_label += "Draft";
    } else {
        date_label += "Unavailable";
    }
    preview_date_->SetText(date_label.c_str());

    std::vector<std::string> state_labels;
    state_labels.push_back(detail.unread ? "Unread" : "Read");
    state_labels.push_back(detail.download_complete ? "Complete" : "Partial");
    if (detail.attachments_omitted) {
        state_labels.push_back("Attachment fetch required");
    }
    if (detail.flagged) {
        state_labels.push_back("Flagged");
    }
    if (detail.deleted) {
        state_labels.push_back("Deleted");
    }
    if (detail.answered) {
        state_labels.push_back("Answered");
    }
    if (!detail.last_error.empty()) {
        state_labels.push_back("Last error: " + detail.last_error);
    }
    if (!preview_render_notice_.empty()) {
        state_labels.push_back(preview_render_notice_);
    }
    preview_state_->SetText(("State: " + JoinBulletList(state_labels)).c_str());
}

std::optional<hermes::MessageRenderRequest> HaikuMainWindow::BuildRenderRequest(
    const hermes::MessageDetail& detail) const {
    hermes::MessageRenderRequest request;
    request.preferred_mode = hermes::RendererMode::kWebKit;
    request.mailbox_id = detail.mailbox_id;
    request.message_id = detail.id;
    request.html_body = detail.html_body;
    request.plain_text_body = detail.plain_text_body;
    request.rtf_body = detail.rtf_body;
    request.paige_native_body = detail.paige_native_body;
    request.styled_source = detail.styled_source;
    request.styled_fidelity = detail.styled_fidelity;
    request.allow_remote_content = true;
    request.read_only = true;
    for (std::size_t index = 0; index < detail.attachments.size(); ++index) {
        hermes::MessageRenderAttachment attachment;
        attachment.name = detail.attachments[index].name;
        attachment.content_type = detail.attachments[index].content_type;
        attachment.content_id = detail.attachments[index].content_id;
        attachment.disposition = detail.attachments[index].disposition;
        attachment.download_complete = detail.attachments[index].download_complete;
        if (const auto payload_path = shell_host_.AttachmentPath(detail.mailbox_id, detail.id, index)) {
            attachment.payload_path = *payload_path;
        }
        request.attachments.push_back(std::move(attachment));
    }
    return request;
}

void HaikuMainWindow::PopulatePreviewBody(const hermes::MessageDetail& detail) {
    const std::string existing_notice = preview_render_notice_;
    const bool styled =
        !detail.html_body.empty() || !detail.rtf_body.empty() || !detail.paige_native_body.empty() ||
        detail.styled_source != hermes::StyledDocumentSource::kPlainText;
    if (styled) {
        const auto request = BuildRenderRequest(detail);
        if (request && preview_web_view_->Load(*request)) {
            preview_plain_root_->Hide();
            preview_web_view_->Show();
            preview_render_notice_ = existing_notice;
            return;
        }
        preview_render_notice_ = existing_notice.empty()
                                     ? "Styled render unavailable; showing plain text"
                                     : existing_notice + "; styled render unavailable; showing plain text";
    } else {
        preview_render_notice_ = existing_notice;
    }
    preview_web_view_->Hide();
    preview_plain_root_->Show();
    preview_text_->SetText(detail.plain_text_body.empty() ? " " : detail.plain_text_body.c_str());
}

void HaikuMainWindow::PopulatePreviewPlaceholder(std::string message) {
    preview_subject_->SetText("Subject: ");
    preview_from_->SetText("From: ");
    preview_to_->SetText("To: ");
    preview_date_->SetText("Date: ");
    preview_state_->SetText("State: ");
    preview_render_notice_.clear();
    preview_web_view_->Hide();
    preview_plain_root_->Show();
    preview_text_->SetText(message.c_str());
    PopulateAttachments(nullptr);
}

void HaikuMainWindow::PopulatePreview() {
    SchedulePreviewRefresh();
}

void HaikuMainWindow::PopulatePreviewNow() {
    preview_refresh_runner_.reset();
    if (!gui_preferences_.show_preview_pane) {
        shell_host_.SetActiveMessageContext(current_mailbox_id_, current_message_id_);
        CancelPreviewRead();
        UpdateCommandState();
        return;
    }

    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        shell_host_.SetActiveMessageContext(current_mailbox_id_, {});
        PopulatePreviewPlaceholder("Select a message or draft to preview it.");
        CancelPreviewRead();
        UpdateCommandState();
        return;
    }
    if (selected_ids.size() > 1) {
        current_message_id_ = selected_ids.front();
        shell_host_.SetActiveMessageContext(current_mailbox_id_, {});
        PopulatePreviewPlaceholder("Select a single message to preview it.");
        CancelPreviewRead();
        UpdateCommandState();
        return;
    }

    current_message_id_ = selected_ids.front();
    shell_host_.SetActiveMessageContext(current_mailbox_id_, current_message_id_);
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail) {
        shell_host_.SetActiveMessageContext(current_mailbox_id_, {});
        PopulatePreviewPlaceholder("Message details unavailable.");
        CancelPreviewRead();
        UpdateCommandState();
        return;
    }

    preview_render_notice_.clear();
    if (const auto mailbox = shell_host_.Mailboxes().GetMailbox(detail->mailbox_id);
        mailbox && mailbox->protocol == MailboxProtocol::kImap && !detail->download_complete) {
        if (preview_fetch_attempt_mailbox_id_ != detail->mailbox_id || preview_fetch_attempt_message_id_ != detail->id) {
            preview_render_notice_ = "Queued IMAP fetch for full preview; showing cached content";
            SchedulePreviewFetch(detail->mailbox_id, detail->id);
        } else {
            preview_render_notice_ = "Partial IMAP message; showing cached content";
        }
    } else {
        preview_fetch_attempt_mailbox_id_.clear();
        preview_fetch_attempt_message_id_.clear();
    }

    PopulatePreviewBody(*detail);
    PopulatePreviewHeader(*detail);
    PopulateAttachments(&*detail);
    SchedulePreviewRead();
    UpdateCommandState();
}

void HaikuMainWindow::PopulateAttachments(const hermes::MessageDetail* detail) {
    attachment_list_->MakeEmpty();
    attachment_indices_.clear();
    if (detail == nullptr) {
        attachment_container_->Hide();
        return;
    }

    for (std::size_t index = 0; index < detail->attachments.size(); ++index) {
        attachment_list_->AddItem(new BStringItem(AttachmentLabel(detail->attachments[index]).c_str()));
        attachment_indices_.push_back(index);
    }

    if (detail->attachments.empty()) {
        attachment_container_->Hide();
    } else {
        attachment_container_->Show();
    }
}

void HaikuMainWindow::PopulateTaskStatus() {
    task_entries_.clear();

    for (const auto& task : shell_host_.Tasks().Tasks()) {
        task_entries_.push_back({task.id, false, false, false});
    }

    for (const auto& action : shell_host_.QueuedImapActions()) {
        task_entries_.push_back({action.id,
                                 true,
                                 action.state == ImapActionState::kPending ||
                                     action.state == ImapActionState::kFailed ||
                                     action.state == ImapActionState::kCancelled,
                                 action.state == ImapActionState::kPending ||
                                     action.state == ImapActionState::kFailed});
    }

    PopulateTaskErrors();
}

void HaikuMainWindow::PopulateTaskErrors() {
    task_error_entries_.clear();

    for (const auto& error : shell_host_.Tasks().Errors()) {
        std::string summary = error.task_id + " [" + ToString(error.kind);
        if (!error.mechanism.empty()) {
            summary += "/" + error.mechanism;
        }
        summary += "]";
        task_error_entries_.push_back({summary, error.message});
    }
    for (const auto& action : shell_host_.QueuedImapActions()) {
        if (!action.last_error.empty()) {
            const std::string summary = ActionKindLabel(action.kind) + " [" + action.id + "]";
            task_error_entries_.push_back({summary, action.last_error});
        }
    }

    const bool tasks_changed = task_entries_.size() != last_task_row_count_;
    const bool errors_changed = task_error_entries_.size() != last_error_row_count_;
    last_task_row_count_ = task_entries_.size();
    last_error_row_count_ = task_error_entries_.size();
    RefreshTaskUtilityFocus(tasks_changed, errors_changed);
}

void HaikuMainWindow::ApplyGuiPreferences() {
    if (toolbar_view_ != nullptr && gui_preferences_.show_toolbar) {
        toolbar_view_->Show();
    } else if (toolbar_view_ != nullptr) {
        toolbar_view_->Hide();
    }

    if (search_bar_container_ != nullptr && gui_preferences_.show_search_bar) {
        search_bar_container_->Show();
    } else if (search_bar_container_ != nullptr) {
        search_bar_container_->Hide();
    }
    if (search_bar_query_control_ != nullptr) {
        search_bar_query_control_->SetExplicitMinSize(
            BSize(static_cast<float>(gui_preferences_.search_bar_width), B_SIZE_UNSET));
    }

    if (preview_container_ != nullptr && gui_preferences_.show_preview_pane) {
        preview_container_->Show();
        SchedulePreviewRefresh();
    } else if (preview_container_ != nullptr) {
        preview_container_->Hide();
        CancelPreviewRead();
        CancelPreviewRefresh();
    }

    if (utility_container_ != nullptr) {
        utility_container_->SetExplicitMinSize(
            BSize(B_SIZE_UNSET, std::max(96, gui_preferences_.utility_pane_height)));
        if (gui_preferences_.utility_pane_open) {
            utility_container_->Show();
        } else {
            utility_container_->Hide();
        }
    }
    if (utility_tabs_ != nullptr) {
        SelectUtilityTab(gui_preferences_.utility_pane_selected_tab);
    }
    UpdateSearchBarState();
    UpdateViewMenuMarks();
    UpdateCommandState();
}

void HaikuMainWindow::PersistGuiPreferences() {
    if (message_list_ != nullptr) {
        gui_preferences_.toc_column_layout = SerializeColumnListState(*message_list_);
    }
    gui_preferences_.utility_pane_open = utility_container_ != nullptr && !utility_container_->IsHidden();
    gui_preferences_.show_preview_pane = preview_container_ != nullptr && !preview_container_->IsHidden();
    gui_preferences_.show_toolbar = toolbar_view_ != nullptr && !toolbar_view_->IsHidden();
    gui_preferences_.show_search_bar = search_bar_container_ != nullptr && !search_bar_container_->IsHidden();
    gui_preferences_.search_bar_mode = search_bar_mode_;
    if (search_bar_query_control_ != nullptr) {
        gui_preferences_.search_bar_width = SearchBarWidthFromSettings(shell_host_.Settings());
        const float frame_width = search_bar_query_control_->Frame().Width();
        if (frame_width > 0.0f) {
            gui_preferences_.search_bar_width =
                std::max(100, std::min(500, static_cast<int>(std::lround(frame_width))));
        }
    }
    if (utility_tabs_ != nullptr) {
        gui_preferences_.utility_pane_selected_tab = utility_tabs_->Selection();
        gui_preferences_.utility_pane_height =
            std::max(96, static_cast<int>(std::lround(utility_tabs_->Frame().Height())));
    }
    ApplyGuiPreferencesToSettings(gui_preferences_, shell_host_.Settings());
    std::string ignored;
    shell_host_.PersistSettings(&ignored);
}

void HaikuMainWindow::UpdateViewMenuMarks() {
    if (show_toolbar_item_ != nullptr) {
        show_toolbar_item_->SetMarked(gui_preferences_.show_toolbar);
    }
    if (show_search_bar_item_ != nullptr) {
        show_search_bar_item_->SetMarked(gui_preferences_.show_search_bar);
    }
    if (show_preview_item_ != nullptr) {
        show_preview_item_->SetMarked(gui_preferences_.show_preview_pane);
    }
    if (show_utility_item_ != nullptr) {
        show_utility_item_->SetMarked(gui_preferences_.tasks_wazoo.open);
    }
    if (group_by_subject_item_ != nullptr) {
        group_by_subject_item_->SetMarked(gui_preferences_.toc_sort.group_by_subject);
    }
    if (work_offline_item_ != nullptr) {
        work_offline_item_->SetMarked(shell_host_.ShellBehavior().offline);
    }
}

void HaikuMainWindow::TogglePreviewPane() {
    gui_preferences_.show_preview_pane = !gui_preferences_.show_preview_pane;
    ApplyGuiPreferences();
    PersistGuiPreferences();
}

void HaikuMainWindow::ToggleToolbar() {
    gui_preferences_.show_toolbar = !gui_preferences_.show_toolbar;
    ApplyGuiPreferences();
    PersistGuiPreferences();
}

void HaikuMainWindow::ToggleSearchBar() {
    gui_preferences_.show_search_bar = !gui_preferences_.show_search_bar;
    ApplyGuiPreferences();
    PersistGuiPreferences();
}

void HaikuMainWindow::ToggleUtilityPane() {
    const bool should_open = !gui_preferences_.tasks_wazoo.open;
    gui_preferences_.tasks_wazoo.open = should_open;
    gui_preferences_.tasks_wazoo.restore_on_launch = should_open;
    ApplyGuiPreferencesToSettings(gui_preferences_, shell_host_.Settings());
    std::string ignored;
    shell_host_.PersistSettings(&ignored);
    shell_host_.SetWazooWindowVisible("tasks", should_open);
    UpdateViewMenuMarks();
}

void HaikuMainWindow::SelectUtilityTab(int32 index) {
    shell_host_.OpenToolWindow(index <= 0 ? "task-status" : "task-errors");
}

void HaikuMainWindow::SetStatusMessage(std::string message) {
    if (status_view_ != nullptr) {
        status_view_->SetText(message.c_str());
    }
}

void HaikuMainWindow::RefreshSearchBarRecentMenu() {
    if (search_bar_recent_menu_ == nullptr) {
        return;
    }

    search_bar_recent_menu_->RemoveItems(0, search_bar_recent_menu_->CountItems(), true);
    const bool has_current_mailbox = !current_mailbox_id_.empty();

    for (std::size_t index = 0; index < gui_preferences_.search_bar_recent_entries.size(); ++index) {
        const auto& entry = gui_preferences_.search_bar_recent_entries[index];
        auto* item = new BMenuItem(
            SearchBarRecentLabel(entry).c_str(), new BMessage(kSearchBarRecentChosenMessage));
        item->Message()->AddInt32("recent_index", static_cast<int32>(index));
        if ((entry.mode == SearchBarMode::kSearchCurrentMailbox ||
             entry.mode == SearchBarMode::kSearchCurrentFolder) &&
            !has_current_mailbox) {
            item->SetEnabled(false);
        }
        search_bar_recent_menu_->AddItem(item);
    }

    if (!gui_preferences_.search_bar_recent_entries.empty()) {
        search_bar_recent_menu_->AddSeparatorItem();
    }

    const auto add_action_item = [this, has_current_mailbox](SearchBarMode mode) {
        auto* item = new BMenuItem(SearchBarModeLabel(mode).c_str(), new BMessage(kSearchBarActionChosenMessage));
        item->Message()->AddInt32("scope_mode", static_cast<int32>(mode));
        if ((mode == SearchBarMode::kSearchCurrentMailbox || mode == SearchBarMode::kSearchCurrentFolder) &&
            !has_current_mailbox) {
            item->SetEnabled(false);
        }
        search_bar_recent_menu_->AddItem(item);
    };
    add_action_item(SearchBarMode::kSearchWeb);
    add_action_item(SearchBarMode::kSearchAllMailboxes);
    add_action_item(SearchBarMode::kSearchCurrentMailbox);
    add_action_item(SearchBarMode::kSearchCurrentFolder);

    search_bar_recent_menu_->SetTargetForItems(this);
}

void HaikuMainWindow::HandleSearchBarRecentSelection(int32 index) {
    if (index < 0 || static_cast<std::size_t>(index) >= gui_preferences_.search_bar_recent_entries.size()) {
        return;
    }
    const auto& entry = gui_preferences_.search_bar_recent_entries[static_cast<std::size_t>(index)];
    search_bar_mode_ = entry.mode;
    SetSearchBarQueryText(entry.text, false);
    UpdateSearchBarState();
    ExecuteSearchBarQuery();
}

void HaikuMainWindow::HandleSearchBarFocusChanged(bool focused) {
    if (search_bar_query_control_ == nullptr || search_bar_query_control_->TextView() == nullptr) {
        return;
    }
    if (focused) {
        if (search_bar_prompt_visible_) {
            const int32 length = static_cast<int32>(std::strlen(search_bar_query_control_->Text()));
            search_bar_query_control_->TextView()->Select(0, length);
        }
        return;
    }

    if (SearchBarQueryText().empty()) {
        SetSearchBarQueryText(SearchBarPromptText(), true);
    }
}

std::string HaikuMainWindow::SearchBarPromptText() const {
    return SearchBarModeLabel(search_bar_mode_);
}

std::string HaikuMainWindow::SearchBarQueryText() const {
    if (search_bar_query_control_ == nullptr || search_bar_query_control_->Text() == nullptr ||
        search_bar_prompt_visible_) {
        return {};
    }
    return TrimWhitespace(search_bar_query_control_->Text());
}

void HaikuMainWindow::SetSearchBarQueryText(std::string_view text, bool prompt_visible) {
    if (search_bar_query_control_ == nullptr) {
        return;
    }
    const std::string value(text);
    updating_search_bar_query_ = true;
    search_bar_prompt_visible_ = prompt_visible;
    search_bar_query_control_->SetText(value.c_str());
    updating_search_bar_query_ = false;
}

void HaikuMainWindow::UpdateSearchBarState() {
    if (search_bar_scope_menu_ == nullptr || search_bar_button_ == nullptr || search_bar_query_control_ == nullptr) {
        return;
    }

    BMenuItem* web_item = search_bar_scope_menu_->ItemAt(0);
    BMenuItem* all_item = search_bar_scope_menu_->ItemAt(1);
    BMenuItem* current_item = search_bar_scope_menu_->ItemAt(2);
    BMenuItem* folder_item = search_bar_scope_menu_->ItemAt(3);
    bool has_current_mailbox = !current_mailbox_id_.empty();
    if (web_item != nullptr) {
        web_item->SetMarked(search_bar_mode_ == SearchBarMode::kSearchWeb);
    }
    if (all_item != nullptr) {
        all_item->SetMarked(search_bar_mode_ == SearchBarMode::kSearchAllMailboxes);
    }
    if (current_item != nullptr) {
        current_item->SetEnabled(has_current_mailbox);
        current_item->SetMarked(search_bar_mode_ == SearchBarMode::kSearchCurrentMailbox && has_current_mailbox);
    }
    if (folder_item != nullptr) {
        folder_item->SetEnabled(has_current_mailbox);
        folder_item->SetMarked(search_bar_mode_ == SearchBarMode::kSearchCurrentFolder && has_current_mailbox);
    }
    if ((search_bar_mode_ == SearchBarMode::kSearchCurrentMailbox ||
         search_bar_mode_ == SearchBarMode::kSearchCurrentFolder) &&
        !has_current_mailbox) {
        search_bar_mode_ = SearchBarMode::kSearchAllMailboxes;
        if (all_item != nullptr) {
            all_item->SetMarked(true);
        }
    }

    if (search_bar_prompt_visible_) {
        const std::string prompt = SearchBarPromptText();
        if (search_bar_query_control_->Text() == nullptr ||
            prompt != std::string(search_bar_query_control_->Text())) {
            SetSearchBarQueryText(prompt, true);
        }
    } else if (SearchBarQueryText().empty() &&
               (search_bar_query_control_->TextView() == nullptr || !search_bar_query_control_->TextView()->IsFocus())) {
        SetSearchBarQueryText(SearchBarPromptText(), true);
    }

    if (search_bar_query_control_ != nullptr) {
        search_bar_query_control_->SetExplicitMinSize(
            BSize(static_cast<float>(gui_preferences_.search_bar_width), B_SIZE_UNSET));
    }
    search_bar_button_->SetLabel("Search");
    search_bar_button_->SetEnabled(!SearchBarQueryText().empty());
    RefreshSearchBarRecentMenu();
}

void HaikuMainWindow::ExecuteSearchBarQuery() {
    const std::string query = SearchBarQueryText();
    if (query.empty()) {
        SetStatusMessage(search_bar_mode_ == SearchBarMode::kSearchWeb ? "Enter text to search the web."
                                                                       : "Enter text to search local mail.");
        return;
    }

    RememberSearchBarRecentEntry(&gui_preferences_.search_bar_recent_entries,
                                 search_bar_mode_,
                                 query,
                                 SearchBarMaxRecentEntriesFromSettings(shell_host_.Settings()));
    RefreshSearchBarRecentMenu();

    if (search_bar_mode_ == SearchBarMode::kSearchWeb) {
        const std::string target = BuildSearchBarWebUrl(shell_host_.Settings(), query);
        if (!LaunchExternalTarget(target)) {
            SetStatusMessage("Unable to open the configured web search.");
            return;
        }
        PersistGuiPreferences();
        SetStatusMessage("Opened web search.");
        SetSearchBarQueryText(SearchBarPromptText(), true);
        return;
    }

    HaikuShellHost::SearchRequest request;
    request.term = query;
    request.scope = HaikuShellHost::SearchRequest::Scope::kAllMailboxes;
    if (search_bar_mode_ == SearchBarMode::kSearchCurrentMailbox) {
        if (current_mailbox_id_.empty()) {
            SetStatusMessage("No active mailbox is available for mailbox-scoped search.");
            return;
        }
        request.scope = HaikuShellHost::SearchRequest::Scope::kCurrentMailbox;
        request.anchor_mailbox_id = current_mailbox_id_;
    } else if (search_bar_mode_ == SearchBarMode::kSearchCurrentFolder) {
        if (current_mailbox_id_.empty()) {
            SetStatusMessage("No active mailbox is available for folder-scoped search.");
            return;
        }
        request.scope = HaikuShellHost::SearchRequest::Scope::kCurrentFolder;
        request.anchor_mailbox_id = current_mailbox_id_;
    }

    shell_host_.QueuePendingSearch(std::move(request));
    shell_host_.OpenToolWindow("search");
    PersistGuiPreferences();
    switch (search_bar_mode_) {
        case SearchBarMode::kSearchAllMailboxes:
            SetStatusMessage("Opened search across all mailboxes.");
            break;
        case SearchBarMode::kSearchCurrentMailbox:
            SetStatusMessage("Opened mailbox-scoped search.");
            break;
        case SearchBarMode::kSearchCurrentFolder:
            SetStatusMessage("Opened current-folder search.");
            break;
        case SearchBarMode::kSearchWeb:
            break;
    }
    SetSearchBarQueryText(SearchBarPromptText(), true);
}

void HaikuMainWindow::RefreshTaskUtilityFocus(bool tasks_changed, bool errors_changed) {
    if (errors_changed && !task_error_entries_.empty() && gui_preferences_.bring_task_error_to_front) {
        shell_host_.OpenToolWindow("task-errors");
        return;
    }
    if (tasks_changed && !task_entries_.empty() && gui_preferences_.bring_task_status_to_front) {
        shell_host_.OpenToolWindow("task-status");
    }
}

void HaikuMainWindow::RebuildToolbar() {
    if (toolbar_view_ == nullptr) {
        return;
    }
    const auto actions = MainToolbarActionSpecs(shell_host_);
    const auto allowed_entries = ToolbarAllowedEntries(actions);
    const auto configuration = ParseToolbarConfiguration(gui_preferences_.main_toolbar_layout,
                                                         allowed_entries,
                                                         MainToolbarDefaultEntries());
    PopulateToolbar(*static_cast<BToolBar*>(toolbar_view_),
                    this,
                    actions,
                    configuration,
                    gui_preferences_.show_toolbar_tips,
                    gui_preferences_.show_toolbar_large_buttons);
    UpdateCommandState();
}

void HaikuMainWindow::UpdateDynamicCommandLabels() {
    const auto shell_behavior = shell_host_.ShellBehavior();
    const auto mail_transfer = shell_host_.MailTransfer();

    if (reply_item_ != nullptr) {
        reply_item_->SetLabel(ReplyMenuLabel(false));
        reply_item_->SetShortcut(shell_behavior.reply_ctrl_r_to_all ? 0 : 'R',
                                 shell_behavior.reply_ctrl_r_to_all ? 0 : B_COMMAND_KEY);
    }
    if (reply_all_item_ != nullptr) {
        reply_all_item_->SetLabel(ReplyMenuLabel(true));
        reply_all_item_->SetShortcut(shell_behavior.reply_ctrl_r_to_all ? 'R' : 0,
                                     shell_behavior.reply_ctrl_r_to_all ? B_COMMAND_KEY : 0);
    }
    if (send_immediately_item_ != nullptr) {
        send_immediately_item_->SetLabel(SendImmediateMenuLabel(mail_transfer.immediate_send));
        send_immediately_item_->SetShortcut('E', B_COMMAND_KEY);
    }
}

HaikuMainWindow::SelectedCommandState HaikuMainWindow::ComputeSelectedCommandState() const {
    SelectedCommandState state;
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return state;
    }

    std::vector<MessageRecord> selected_records;
    selected_records.reserve(selected_ids.size());
    for (const auto& message_id : selected_ids) {
        if (const auto record = shell_host_.Messages().GetMessage(current_mailbox_id_, message_id)) {
            selected_records.push_back(*record);
        }
    }
    if (selected_records.empty()) {
        return state;
    }

    state.has_selection = true;
    state.unanimous_status = [&selected_records]() -> std::optional<LegacyMessageStatus> {
        const auto value = selected_records.front().legacy_status;
        return std::all_of(selected_records.begin(), selected_records.end(), [&](const auto& record) {
                   return record.legacy_status == value;
               })
                   ? std::optional<LegacyMessageStatus>(value)
                   : std::nullopt;
    }();
    state.unanimous_label = [&selected_records]() -> std::optional<int> {
        const int value = selected_records.front().label_index;
        return std::all_of(selected_records.begin(), selected_records.end(), [&](const auto& record) {
                   return record.label_index == value;
               })
                   ? std::optional<int>(value)
                   : std::nullopt;
    }();
    state.unanimous_priority = [&selected_records]() -> std::optional<int> {
        const int value = static_cast<int>(selected_records.front().compose_options.priority);
        return std::all_of(selected_records.begin(), selected_records.end(), [&](const auto& record) {
                   return static_cast<int>(record.compose_options.priority) == value;
               })
                   ? std::optional<int>(value)
                   : std::nullopt;
    }();
    state.unanimous_pop_status = [&selected_records]() -> std::optional<PopServerStatus> {
        const auto value = selected_records.front().pop_server_status;
        return std::all_of(selected_records.begin(), selected_records.end(), [&](const auto& record) {
                   return record.pop_server_status == value;
               })
                   ? std::optional<PopServerStatus>(value)
                   : std::nullopt;
    }();

    state.all_read = std::all_of(selected_records.begin(), selected_records.end(), [](const auto& record) {
        return !record.unread;
    });
    state.all_unread = std::all_of(selected_records.begin(), selected_records.end(), [](const auto& record) {
        return record.unread;
    });

    return state;
}

void HaikuMainWindow::UpdateCheckedCommandState() {
    const auto clear_marks_for = [this](uint32 command) {
        if (const auto it = command_menu_items_.find(command); it != command_menu_items_.end()) {
            for (auto* item : it->second) {
                if (item != nullptr) {
                    item->SetMarked(false);
                }
            }
        }
    };

    clear_marks_for(kMarkReadMessage);
    clear_marks_for(kMarkUnreadMessage);
    clear_marks_for(kSetLegacyStatusMessage);
    clear_marks_for(kSetLabelMessage);
    clear_marks_for(kServerLeaveMessage);
    clear_marks_for(kServerFetchMessage);
    clear_marks_for(kServerDeleteMessage);
    clear_marks_for(kServerFetchDeleteMessage);
    clear_marks_for(kPriorityHighestMessage);
    clear_marks_for(kPriorityHighMessage);
    clear_marks_for(kPriorityNormalMessage);
    clear_marks_for(kPriorityLowMessage);
    clear_marks_for(kPriorityLowestMessage);

    const SelectedCommandState state = ComputeSelectedCommandState();
    if (!state.has_selection) {
        return;
    }

    if (const auto it = command_menu_items_.find(kMarkReadMessage); it != command_menu_items_.end()) {
        for (auto* item : it->second) {
            if (item != nullptr) {
                item->SetMarked(state.all_read);
            }
        }
    }
    if (const auto it = command_menu_items_.find(kMarkUnreadMessage); it != command_menu_items_.end()) {
        for (auto* item : it->second) {
            if (item != nullptr) {
                item->SetMarked(state.all_unread);
            }
        }
    }
    if (state.unanimous_status) {
        if (const auto it = command_menu_items_.find(kSetLegacyStatusMessage); it != command_menu_items_.end()) {
            for (auto* item : it->second) {
                if (item == nullptr || item->Message() == nullptr) {
                    continue;
                }
                int32 status = -1;
                if (item->Message()->FindInt32("legacy_status", &status) == B_OK) {
                    item->SetMarked(status == static_cast<int32>(*state.unanimous_status));
                }
            }
        }
    }
    if (state.unanimous_label) {
        if (const auto it = command_menu_items_.find(kSetLabelMessage); it != command_menu_items_.end()) {
            for (auto* item : it->second) {
                if (item == nullptr || item->Message() == nullptr) {
                    continue;
                }
                int32 label_index = -1;
                if (item->Message()->FindInt32("label_index", &label_index) == B_OK) {
                    item->SetMarked(label_index == *state.unanimous_label);
                }
            }
        }
    }
    if (state.unanimous_priority) {
        const std::vector<std::pair<uint32, int>> priorities = {
            {kPriorityHighestMessage, static_cast<int>(ComposePriority::kHighest)},
            {kPriorityHighMessage, static_cast<int>(ComposePriority::kHigh)},
            {kPriorityNormalMessage, static_cast<int>(ComposePriority::kNormal)},
            {kPriorityLowMessage, static_cast<int>(ComposePriority::kLow)},
            {kPriorityLowestMessage, static_cast<int>(ComposePriority::kLowest)},
        };
        for (const auto& [command, value] : priorities) {
            if (const auto it = command_menu_items_.find(command); it != command_menu_items_.end()) {
                for (auto* item : it->second) {
                    if (item != nullptr) {
                        item->SetMarked(value == *state.unanimous_priority);
                    }
                }
            }
        }
    }
    if (state.unanimous_pop_status) {
        const std::vector<std::pair<uint32, PopServerStatus>> statuses = {
            {kServerLeaveMessage, PopServerStatus::kLeave},
            {kServerFetchMessage, PopServerStatus::kFetch},
            {kServerDeleteMessage, PopServerStatus::kDelete},
            {kServerFetchDeleteMessage, PopServerStatus::kFetchDelete},
        };
        for (const auto& [command, status] : statuses) {
            if (const auto it = command_menu_items_.find(command); it != command_menu_items_.end()) {
                for (auto* item : it->second) {
                    if (item != nullptr) {
                        item->SetMarked(status == *state.unanimous_pop_status);
                    }
                }
            }
        }
    }
}

void HaikuMainWindow::UpdateCommandState() {
    UpdateDynamicCommandLabels();
    for (const auto& [command, items] : command_menu_items_) {
        const bool enabled = IsCommandEnabled(command);
        for (auto* item : items) {
            if (item != nullptr) {
                item->SetEnabled(enabled);
            }
        }
    }

    UpdateCommandControls(toolbar_view_, [this](uint32 command) { return IsCommandEnabled(command); });
    UpdateCheckedCommandState();
    if (new_message_to_menu_ != nullptr && new_message_to_menu_->Superitem() != nullptr) {
        new_message_to_menu_->Superitem()->SetEnabled(new_message_to_menu_->CountItems() > 0 &&
                                                      new_message_to_menu_->ItemAt(0) != nullptr &&
                                                      new_message_to_menu_->ItemAt(0)->Message() != nullptr);
    }
    if (forward_to_menu_ != nullptr && forward_to_menu_->Superitem() != nullptr) {
        forward_to_menu_->Superitem()->SetEnabled(IsCommandEnabled(kForwardMessage) &&
                                                  forward_to_menu_->CountItems() > 0 &&
                                                  forward_to_menu_->ItemAt(0) != nullptr &&
                                                  forward_to_menu_->ItemAt(0)->Message() != nullptr);
    }
    if (redirect_to_menu_ != nullptr && redirect_to_menu_->Superitem() != nullptr) {
        redirect_to_menu_->Superitem()->SetEnabled(IsCommandEnabled(kRedirectMessage) &&
                                                   redirect_to_menu_->CountItems() > 0 &&
                                                   redirect_to_menu_->ItemAt(0) != nullptr &&
                                                   redirect_to_menu_->ItemAt(0)->Message() != nullptr);
    }
    if (new_message_with_menu_ != nullptr && new_message_with_menu_->Superitem() != nullptr) {
        new_message_with_menu_->Superitem()->SetEnabled(true);
    }
    if (reply_with_menu_ != nullptr && reply_with_menu_->Superitem() != nullptr) {
        reply_with_menu_->Superitem()->SetEnabled(IsCommandEnabled(kReplyMessage));
    }
    if (reply_all_with_menu_ != nullptr && reply_all_with_menu_->Superitem() != nullptr) {
        reply_all_with_menu_->Superitem()->SetEnabled(IsCommandEnabled(kReplyAllMessage));
    }
    if (change_personality_menu_ != nullptr && change_personality_menu_->Superitem() != nullptr) {
        change_personality_menu_->Superitem()->SetEnabled(current_mailbox_id_ == "out" && !SelectedMessageIds().empty());
    }
}

void HaikuMainWindow::EnsureCtrlJMappingResolved() {
    mailbox_ui_ = shell_host_.MailboxUi();
    if (mailbox_ui_.ctrl_j_mapping != CtrlJMapping::kUnknown) {
        return;
    }

    if (!shell_host_.Filters().Rules().empty()) {
        const int32 choice = BAlert("ctrl-j-mapping",
                                    "Manual filters are already present. Should Command-J mark mail as junk or run "
                                    "Filter Messages?",
                                    "Junk",
                                    "Filter")
                                 ->Go();
        mailbox_ui_.ctrl_j_mapping = choice == 1 ? CtrlJMapping::kFilter : CtrlJMapping::kJunk;
    } else {
        mailbox_ui_.ctrl_j_mapping = CtrlJMapping::kJunk;
    }

    ApplyMailboxUiSettingsToSettings(mailbox_ui_, shell_host_.Settings());
    std::string ignored;
    shell_host_.PersistSettings(&ignored);
}

std::vector<std::string> HaikuMainWindow::SelectedMessageIds() const {
    std::vector<std::string> ids;
    if (message_list_ == nullptr) {
        return ids;
    }
    BRow* selected = nullptr;
    while ((selected = message_list_->CurrentSelection(selected)) != nullptr) {
        if (const auto* row = dynamic_cast<const TocMessageRow*>(selected)) {
            ids.push_back(row->message_id);
        }
    }
    return ids;
}

std::size_t HaikuMainWindow::SelectedMessageCount() const {
    return SelectedMessageIds().size();
}

std::optional<std::string> HaikuMainWindow::FirstSelectedMessageId() const {
    if (auto ids = SelectedMessageIds(); !ids.empty()) {
        return ids.front();
    }
    return std::nullopt;
}

void HaikuMainWindow::ShowFindWindow() {
    last_find_focus_view_ = CurrentFocus();
    if (find_window_ == nullptr) {
        find_window_ = new HaikuFindWindow(BMessenger(this), kFindConfirmedMessage, kFindClosedMessage, "Find");
    }
    if (auto* find_window = dynamic_cast<HaikuFindWindow*>(find_window_)) {
        if (HasSharedFindQuery()) {
            find_window->SetQuery(SharedFindQuery());
        }
        if (find_window->IsHidden()) {
            find_window->Show();
        } else {
            find_window->Activate(true);
        }
        find_window->FocusQuery();
    }
}

bool HaikuMainWindow::FindInPreview(std::string_view query, bool repeat) {
    if (preview_web_view_ != nullptr && !preview_web_view_->IsHidden()) {
        const auto result = preview_web_view_->Find(query, {});
        if (result.matched) {
            preview_web_view_->MakeFocus(true);
            return true;
        }
    }
    if (preview_text_ == nullptr || preview_plain_root_ == nullptr || preview_plain_root_->IsHidden() ||
        query.empty() || !current_message_id_.size()) {
        return false;
    }
    const char* text = preview_text_->Text();
    if (text == nullptr) {
        return false;
    }
    const std::string haystack = ToLower(text);
    const std::string needle = ToLower(std::string(query));
    int32 start = 0;
    int32 end = 0;
    preview_text_->GetSelection(&start, &end);
    std::size_t offset = repeat && end > start ? static_cast<std::size_t>(end) : 0;
    std::size_t match = haystack.find(needle, offset);
    if (match == std::string::npos && offset > 0) {
        match = haystack.find(needle);
    }
    if (match == std::string::npos) {
        return false;
    }
    preview_text_->MakeFocus(true);
    preview_text_->Select(static_cast<int32>(match), static_cast<int32>(match + needle.size()));
    preview_text_->ScrollToSelection();
    return true;
}

bool HaikuMainWindow::FindInToc(std::string_view query, bool repeat) {
    if (message_list_ == nullptr || query.empty() || message_list_->CountRows() <= 0) {
        return false;
    }
    const std::string needle = ToLower(std::string(query));
    int32 start_index = 0;
    if (const auto first_selected = FirstSelectedMessageId()) {
        for (int32 index = 0; const auto* row = dynamic_cast<const TocMessageRow*>(message_list_->RowAt(index)); ++index) {
            if (row->message_id == *first_selected) {
                start_index = repeat ? index + 1 : index;
                break;
            }
        }
    }

    const int32 row_count = message_list_->CountRows();
    for (int32 step = 0; step < row_count; ++step) {
        const int32 index = (start_index + step) % row_count;
        if (auto* row = dynamic_cast<TocMessageRow*>(message_list_->RowAt(index));
            row != nullptr && row->searchable_text.find(needle) != std::string::npos) {
            message_list_->DeselectAll();
            message_list_->SetFocusRow(row, true);
            current_message_id_ = row->message_id;
            PopulatePreviewNow();
            return true;
        }
    }
    return false;
}

bool HaikuMainWindow::HandleTextFind(std::string_view query, bool repeat) {
    switch (ActiveFindTarget()) {
        case MailboxFindTarget::kPreview:
            return FindInPreview(query, repeat);
        case MailboxFindTarget::kToc:
            return FindInToc(query, repeat);
    }
    return false;
}

HaikuMainWindow::MailboxFindTarget HaikuMainWindow::ActiveFindTarget() const {
    if (IsFocusWithin(preview_web_view_) ||
        (preview_text_ != nullptr && preview_plain_root_ != nullptr && !preview_plain_root_->IsHidden() &&
         IsFocusWithin(preview_text_))) {
        return MailboxFindTarget::kPreview;
    }
    return MailboxFindTarget::kToc;
}

bool HaikuMainWindow::IsFocusWithin(const BView* ancestor) const {
    if (ancestor == nullptr) {
        return false;
    }
    for (const BView* focus = CurrentFocus(); focus != nullptr; focus = focus->Parent()) {
        if (focus == ancestor) {
            return true;
        }
    }
    return false;
}

bool HaikuMainWindow::ShouldShowJunkColumn() const {
    const auto mailbox = current_mailbox_id_.empty() ? std::optional<MailboxRecord>()
                                                     : shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
    const bool junk_mailbox =
        mailbox && (ToLower(mailbox->display_name) == "junk" || ToLower(mailbox->id) == "junk");
    return mailbox_ui_.mailbox_show_junk || junk_mailbox;
}

bool HaikuMainWindow::ShouldShowLabelColumn() const {
    return mailbox_ui_.mailbox_show_label;
}

bool HaikuMainWindow::ShouldShowServerStatusColumn() const {
    if (mailbox_ui_.mailbox_show_server_status) {
        return true;
    }
    const auto mailbox = current_mailbox_id_.empty() ? std::optional<MailboxRecord>()
                                                     : shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
    if (mailbox && mailbox->protocol == MailboxProtocol::kLocal) {
        if (const auto account = shell_host_.Accounts().FindById(mailbox->account_id); account && account->uses_pop) {
            return true;
        }
    }
    const auto messages = shell_host_.Workspace().MessagesForMailbox(current_mailbox_id_);
    return std::any_of(messages.begin(), messages.end(), [](const MessageSummary& message) {
        return message.pop_server_status != PopServerStatus::kNone;
    });
}

void HaikuMainWindow::UpdateTocColumnVisibility() {
    if (status_column_ != nullptr) {
        status_column_->SetVisible(mailbox_ui_.mailbox_show_status);
    }
    if (junk_column_ != nullptr) {
        junk_column_->SetVisible(ShouldShowJunkColumn());
    }
    if (label_column_ != nullptr) {
        label_column_->SetVisible(ShouldShowLabelColumn());
    }
    if (pop_server_status_column_ != nullptr) {
        pop_server_status_column_->SetVisible(ShouldShowServerStatusColumn());
    }
}

void HaikuMainWindow::RefreshDynamicMessageMenus() {
    const auto rebuild_menu = [](BMenu* menu) {
        if (menu != nullptr) {
            menu->RemoveItems(0, menu->CountItems(), true);
        }
    };
    rebuild_menu(new_message_to_menu_);
    rebuild_menu(forward_to_menu_);
    rebuild_menu(redirect_to_menu_);
    rebuild_menu(new_message_with_menu_);
    rebuild_menu(reply_with_menu_);
    rebuild_menu(reply_all_with_menu_);
    rebuild_menu(change_personality_menu_);

    const auto populate_recipient_menu = [this](BMenu* menu, uint32 command) {
        if (menu == nullptr) {
            return;
        }
        for (const auto& entry : shell_host_.Nicknames().Entries()) {
            auto* message = new BMessage(command);
            message->AddString("nickname_name", entry.nickname.c_str());
            menu->AddItem(new BMenuItem(entry.nickname.c_str(), message));
        }
        if (menu->CountItems() == 0) {
            auto* empty = new BMenuItem("(No nicknames)", nullptr);
            empty->SetEnabled(false);
            menu->AddItem(empty);
        }
    };
    populate_recipient_menu(new_message_to_menu_, kDynamicNewRecipientMessage);
    populate_recipient_menu(forward_to_menu_, kDynamicForwardRecipientMessage);
    populate_recipient_menu(redirect_to_menu_, kDynamicRedirectRecipientMessage);

    if (new_message_with_menu_ != nullptr) {
        for (const auto& stationery : shell_host_.Stationery().List()) {
            auto* message = new BMessage(kDynamicNewStationeryMessage);
            message->AddString("stationery_name", stationery.name.c_str());
            new_message_with_menu_->AddItem(new BMenuItem(stationery.name.c_str(), message));
        }
        if (new_message_with_menu_->CountItems() == 0) {
            auto* empty = new BMenuItem("(No stationery)", nullptr);
            empty->SetEnabled(false);
            new_message_with_menu_->AddItem(empty);
        }
    }
    if (reply_with_menu_ != nullptr) {
        for (const auto& stationery : shell_host_.Stationery().List()) {
            auto* message = new BMessage(kDynamicReplyStationeryMessage);
            message->AddString("stationery_name", stationery.name.c_str());
            reply_with_menu_->AddItem(new BMenuItem(stationery.name.c_str(), message));
        }
        if (reply_with_menu_->CountItems() == 0) {
            auto* empty = new BMenuItem("(No stationery)", nullptr);
            empty->SetEnabled(false);
            reply_with_menu_->AddItem(empty);
        }
    }
    if (reply_all_with_menu_ != nullptr) {
        for (const auto& stationery : shell_host_.Stationery().List()) {
            auto* message = new BMessage(kDynamicReplyAllStationeryMessage);
            message->AddString("stationery_name", stationery.name.c_str());
            reply_all_with_menu_->AddItem(new BMenuItem(stationery.name.c_str(), message));
        }
        if (reply_all_with_menu_->CountItems() == 0) {
            auto* empty = new BMenuItem("(No stationery)", nullptr);
            empty->SetEnabled(false);
            reply_all_with_menu_->AddItem(empty);
        }
    }
    if (change_personality_menu_ != nullptr) {
        for (const auto& account : shell_host_.Accounts().Accounts()) {
            auto* message = new BMessage(kDynamicChangePersonaMessage);
            message->AddString("persona_id", account.id.c_str());
            const std::string label = account.display_name.empty() ? account.id : account.display_name;
            change_personality_menu_->AddItem(new BMenuItem(label.c_str(), message));
        }
        if (change_personality_menu_->CountItems() == 0) {
            auto* empty = new BMenuItem("(No personalities)", nullptr);
            empty->SetEnabled(false);
            change_personality_menu_->AddItem(empty);
        }
    }
}

std::vector<MailboxRecord> HaikuMainWindow::RecentMailboxEntries(std::optional<std::string_view> account_id,
                                                                 bool imap_only,
                                                                 std::string_view excluded_mailbox_id) {
    auto preferences = GuiPreferencesFromSettings(shell_host_.Settings());
    const auto recent_mailbox_ids =
        hermes::NormalizeRecentMailboxIds(preferences.recent_mailbox_ids,
                                          shell_host_.Mailboxes(),
                                          MaxRecentMailboxCount(shell_host_.Settings()));
    if (recent_mailbox_ids != preferences.recent_mailbox_ids) {
        preferences.recent_mailbox_ids = recent_mailbox_ids;
        ApplyGuiPreferencesToSettings(preferences, shell_host_.Settings());
        std::string ignored;
        shell_host_.PersistSettings(&ignored);
    }
    gui_preferences_.recent_mailbox_ids = recent_mailbox_ids;

    std::vector<MailboxRecord> recent_mailboxes;
    recent_mailboxes.reserve(recent_mailbox_ids.size());
    for (const auto& mailbox_id : recent_mailbox_ids) {
        const auto mailbox = shell_host_.Mailboxes().GetMailbox(mailbox_id);
        if (!mailbox || mailbox->kind != MailboxKind::kMailbox) {
            continue;
        }
        if (!excluded_mailbox_id.empty() && mailbox->id == excluded_mailbox_id) {
            continue;
        }
        if (imap_only && mailbox->protocol != MailboxProtocol::kImap) {
            continue;
        }
        if (account_id && mailbox->account_id != *account_id) {
            continue;
        }
        recent_mailboxes.push_back(*mailbox);
    }
    return recent_mailboxes;
}

void HaikuMainWindow::RefreshRecentMailboxMenu() {
    if (recent_mailboxes_menu_ == nullptr) {
        return;
    }

    recent_mailboxes_menu_->RemoveItems(0, recent_mailboxes_menu_->CountItems(), true);
    for (const auto& mailbox : RecentMailboxEntries(std::nullopt, false, std::string_view{})) {
        auto* message = new BMessage(kOpenRecentMailboxMessage);
        message->AddString("mailbox_id", mailbox.id.c_str());
        recent_mailboxes_menu_->AddItem(new BMenuItem(mailbox.display_name.c_str(), message));
    }

    if (recent_mailboxes_menu_->Superitem() != nullptr) {
        recent_mailboxes_menu_->Superitem()->SetEnabled(recent_mailboxes_menu_->CountItems() > 0);
    }
}

void HaikuMainWindow::ApplyFindShortcutMapping() {
    const bool switched = mailbox_ui_.search_accel_switch;
    if (find_text_item_ != nullptr) {
        find_text_item_->SetShortcut('F', switched ? B_COMMAND_KEY : (B_COMMAND_KEY | B_SHIFT_KEY));
    }
    if (find_messages_item_ != nullptr) {
        find_messages_item_->SetShortcut('F', switched ? (B_COMMAND_KEY | B_SHIFT_KEY) : B_COMMAND_KEY);
    }
}

void HaikuMainWindow::RestoreFindFocus() {
    if (last_find_focus_view_ != nullptr) {
        last_find_focus_view_->MakeFocus(true);
    }
}

void HaikuMainWindow::HandleGroupBySubjectToggle() {
    gui_preferences_.toc_sort.group_by_subject = !gui_preferences_.toc_sort.group_by_subject;
    PersistGuiPreferences();
    PopulateMessagesForCurrentMailbox();
    PopulatePreview();
    UpdateViewMenuMarks();
    SetStatusMessage(gui_preferences_.toc_sort.group_by_subject ? "Grouped messages by subject."
                                                                : "Subject grouping turned off.");
}

std::optional<int32> HaikuMainWindow::TocFieldForColumn(const BColumn* column) const {
    if (column == nullptr) {
        return std::nullopt;
    }
    for (const auto& entry : toc_columns_) {
        if (entry.first == column) {
            return entry.second;
        }
    }
    return std::nullopt;
}

void HaikuMainWindow::HandleTocHeaderClick(BPoint where, uint32 modifiers) {
    if (message_list_ == nullptr) {
        return;
    }
    const auto field = TocFieldForColumn(message_list_->ColumnAt(where));
    if (!field) {
        return;
    }

    const bool add_secondary = (modifiers & B_COMMAND_KEY) != 0;
    const bool descending = (modifiers & B_SHIFT_KEY) != 0;
    auto& toc_sort = gui_preferences_.toc_sort;

    if (!add_secondary) {
        if (toc_sort.primary_field == *field && toc_sort.primary_descending == descending) {
            toc_sort.primary_field = toc_sort.secondary_field;
            toc_sort.primary_descending = toc_sort.secondary_descending;
            toc_sort.secondary_field = -1;
            toc_sort.secondary_descending = false;
        } else {
            toc_sort.primary_field = *field;
            toc_sort.primary_descending = descending;
            if (toc_sort.secondary_field == *field) {
                toc_sort.secondary_field = -1;
                toc_sort.secondary_descending = false;
            }
            toc_sort.secondary_field = -1;
            toc_sort.secondary_descending = false;
        }
    } else {
        if (toc_sort.secondary_field == *field && toc_sort.secondary_descending == descending) {
            toc_sort.secondary_field = -1;
            toc_sort.secondary_descending = false;
        } else if (toc_sort.primary_field != *field) {
            toc_sort.secondary_field = *field;
            toc_sort.secondary_descending = descending;
        } else if (toc_sort.primary_descending != descending) {
            toc_sort.primary_descending = descending;
        }
    }

    PersistGuiPreferences();
    PopulateMessagesForCurrentMailbox();
    PopulatePreview();
    SetStatusMessage("Updated mailbox sort order.");
}

bool HaikuMainWindow::IsCommandEnabled(uint32 command) const {
    const auto shell_behavior = shell_host_.ShellBehavior();
    const auto mailbox = current_mailbox_id_.empty() ? std::optional<MailboxRecord>()
                                                     : shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
    const auto selected_ids = SelectedMessageIds();
    const bool has_selection = !selected_ids.empty();
    const bool has_single_selection = selected_ids.size() == 1;
    const auto detail = has_single_selection ? shell_host_.WorkspaceMessageDetail(selected_ids.front())
                                             : std::optional<hermes::MessageDetail>();
    const bool has_mailbox = static_cast<bool>(mailbox);
    const bool has_message = has_selection;
    const bool imap_mailbox = has_mailbox && mailbox->protocol == MailboxProtocol::kImap;
    const bool junk_mailbox = has_mailbox &&
                              (ToLower(mailbox->display_name) == "junk" || ToLower(mailbox->id) == "junk");
    const bool out_mailbox = current_mailbox_id_ == "out";

    const auto has_deleted_messages = [this]() {
        for (const auto& message : shell_host_.Messages().ListMessages(current_mailbox_id_)) {
            if (message.deleted) {
                return true;
            }
        }
        return false;
    };

    const auto has_imap_destinations = [this, &mailbox]() {
        if (!mailbox || mailbox->protocol != MailboxProtocol::kImap) {
            return false;
        }
        for (const auto& candidate : shell_host_.Mailboxes().ListMailboxes()) {
            if (candidate.protocol == MailboxProtocol::kImap && candidate.account_id == mailbox->account_id &&
                candidate.id != current_mailbox_id_) {
                return true;
            }
        }
        return false;
    };

    const auto has_pop_tracked_selection = [this, &selected_ids]() {
        if (selected_ids.empty()) {
            return false;
        }
        for (const auto& id : selected_ids) {
            const auto record = shell_host_.Messages().GetMessage(current_mailbox_id_, id);
            if (!record || record->remote_id.empty()) {
                return false;
            }
            const auto account = shell_host_.Accounts().FindById(record->account_id);
            if (!account || !account->uses_pop) {
                return false;
            }
        }
        return true;
    };

    const auto has_out_queue_selection = [this, &selected_ids, out_mailbox]() {
        if (!out_mailbox || selected_ids.empty()) {
            return false;
        }
        for (const auto& id : selected_ids) {
            const auto record = shell_host_.Messages().GetMessage("out", id);
            if (!record) {
                return false;
            }
            if (record->delivery_state != MessageDeliveryState::kQueued &&
                record->delivery_state != MessageDeliveryState::kFailed &&
                record->legacy_status != LegacyMessageStatus::kSendable &&
                record->legacy_status != LegacyMessageStatus::kQueued &&
                record->legacy_status != LegacyMessageStatus::kTimeQueued) {
                return false;
            }
        }
        return true;
    };

    const auto has_imap_fetchable_selection = [this, &selected_ids, imap_mailbox]() {
        if (!imap_mailbox || selected_ids.empty()) {
            return false;
        }
        return std::any_of(selected_ids.begin(), selected_ids.end(), [&](const auto& id) {
            const auto item = shell_host_.Workspace().GetMessage(id);
            return item && !item->download_complete && item->size == 0;
        });
    };

    const auto has_imap_full_fetchable_selection = [this, &selected_ids, imap_mailbox]() {
        if (!imap_mailbox || selected_ids.empty()) {
            return false;
        }
        return std::any_of(selected_ids.begin(), selected_ids.end(), [&](const auto& id) {
            const auto item = shell_host_.Workspace().GetMessage(id);
            return item && item->attachment_count > 0 &&
                   (!item->download_complete || item->attachments_omitted);
        });
    };

    const auto has_imap_redownload_selection = [this, &selected_ids, imap_mailbox]() {
        if (!imap_mailbox || selected_ids.empty()) {
            return false;
        }
        return std::any_of(selected_ids.begin(), selected_ids.end(), [&](const auto& id) {
            return shell_host_.Workspace().GetMessage(id).has_value();
        });
    };

    const auto has_imap_redownload_full_selection = [this, &selected_ids, imap_mailbox]() {
        if (!imap_mailbox || selected_ids.empty()) {
            return false;
        }
        return std::any_of(selected_ids.begin(), selected_ids.end(), [&](const auto& id) {
            const auto item = shell_host_.Workspace().GetMessage(id);
            return item && item->attachment_count > 0;
        });
    };

    const auto has_cached_imap_body_selection = [this, &selected_ids, imap_mailbox]() {
        if (!imap_mailbox || selected_ids.empty()) {
            return false;
        }
        return std::any_of(selected_ids.begin(), selected_ids.end(), [&](const auto& id) {
            const auto item = shell_host_.Workspace().GetMessage(id);
            return item && (item->download_complete || item->size > 0);
        });
    };

    const auto has_local_role_mailbox = [this](std::string_view role_name) {
        for (const auto& candidate : shell_host_.Mailboxes().ListMailboxes()) {
            const std::string lowered_id = ToLower(candidate.id);
            const std::string lowered_name = ToLower(candidate.display_name);
            if (candidate.protocol == MailboxProtocol::kLocal &&
                (lowered_id == role_name || lowered_name == role_name)) {
                return true;
            }
        }
        return false;
    };

    switch (command) {
        case kToolbarOpenMailboxMessage:
        case kToolbarComposeNicknameMessage:
        case kToolbarComposeStationeryMessage:
        case kToolbarComposePersonaMessage:
        case kToolbarRevealPluginMessage:
            return true;
        case kCtrlFMessage:
            return mailbox_ui_.search_accel_switch ? IsCommandEnabled(kFindTextMessage)
                                                   : IsCommandEnabled(kFindMessagesMessage);
        case kCtrlShiftFMessage:
            return mailbox_ui_.search_accel_switch ? IsCommandEnabled(kFindMessagesMessage)
                                                   : IsCommandEnabled(kFindTextMessage);
        case kBackspaceDeleteMessage:
            return shell_behavior.backspace_delete && has_message && IsFocusWithin(message_list_);
        case kTocInvokeMessage:
            return has_single_selection && IsFocusWithin(message_list_);
        case kCheckMailMessage:
        case kSendQueuedMessage:
        case kSendReceiveMessage:
            return !shell_behavior.offline;
        case kStopTasksMessage:
            return true;
        case kFindTextMessage:
            return (has_single_selection &&
                    ((preview_web_view_ != nullptr && !preview_web_view_->IsHidden()) ||
                     (preview_text_ != nullptr && preview_plain_root_ != nullptr &&
                      !preview_plain_root_->IsHidden()))) ||
                   message_list_->CountRows() > 0;
        case kFindAgainMessage:
            return HasSharedFindQuery() && IsCommandEnabled(kFindTextMessage);
        case kRefreshMailboxMessage:
        case kResyncMailboxMessage:
            return imap_mailbox && !shell_behavior.offline;
        case kFindMessagesMessage:
            return has_mailbox;
        case kOpenMessageMessage:
        case kForwardMessage:
        case kRedirectMessage:
        case kSendAgainMessage:
        case kMakeNicknameMessage:
        case kPrintPreviewMessage:
        case kPrintDirectMessage:
            return has_single_selection;
        case kPreviousMessageMessage:
        case kNextMessageMessage:
            return message_list_ != nullptr && message_list_->CountRows() > 0;
        case kSendImmediatelyMessage:
        case kChangeQueueingMessage:
            return has_message && has_out_queue_selection();
        case kToggleStatusMessage:
            return has_message;
        case kReplyMessage:
        case kReplyAllMessage:
        case kDeleteMessageMessage:
        case kMarkReadMessage:
        case kMarkUnreadMessage:
        case kSetLegacyStatusMessage:
        case kSetLabelMessage:
        case kFilterMessagesMessage:
        case kMakeFilterMessage:
            return has_message;
        case kUndeleteMessageMessage:
            return has_message && !shell_behavior.offline && imap_mailbox &&
                   std::any_of(selected_ids.begin(), selected_ids.end(), [&](const auto& id) {
                       const auto item = shell_host_.WorkspaceMessageDetail(id);
                       return item && item->deleted;
                   });
        case kPurgeMailboxMessage:
            return !shell_behavior.offline && imap_mailbox && has_deleted_messages();
        case kMoveMessageMessage:
        case kCopyMessageMessage:
            return has_message && !shell_behavior.offline && has_imap_destinations();
        case kFetchDefaultMessageMessage:
            return has_message && has_imap_fetchable_selection() && !shell_behavior.offline;
        case kFetchFullMessageMessage:
            return has_message && has_imap_full_fetchable_selection() && !shell_behavior.offline;
        case kImapRedownloadDefaultMessage:
            return has_message && has_imap_redownload_selection() && !shell_behavior.offline;
        case kImapRedownloadFullMessage:
            return has_message && has_imap_redownload_full_selection() && !shell_behavior.offline;
        case kImapClearCachedMessage:
            return has_message && has_cached_imap_body_selection() && !shell_behavior.offline;
        case kMarkJunkMessage:
            return has_message && (!junk_mailbox || mailbox_ui_.always_enable_junk);
        case kMarkNotJunkMessage:
            return has_message && (junk_mailbox || mailbox_ui_.always_enable_junk);
        case kRecheckJunkMessage:
            return has_message;
        case kServerLeaveMessage:
        case kServerFetchMessage:
        case kServerDeleteMessage:
        case kServerFetchDeleteMessage:
            return has_pop_tracked_selection() && !shell_behavior.offline;
        case kSpecialWorkOfflineMessage:
        case kSpecialForgetPasswordsMessage:
        case kSpecialChangePasswordMessage:
        case kSpecialOptionsMessage:
            return true;
        case kSpecialEmptyTrashMessage:
            return has_local_role_mailbox("trash");
        case kSpecialTrimJunkMessage:
            return has_local_role_mailbox("junk");
        case kSpecialCompactMailboxesMessage:
            return true;
        case kWindowSendBehindMessage:
        case kWindowCascadeMessage:
        case kWindowTileHorizontalMessage:
        case kWindowTileVerticalMessage:
        case kWindowArrangeMessage:
        case kWindowCloseAllMessage:
            return true;
        case kSelectedTextUrl1Message:
        case kSelectedTextUrl2Message:
        case kSelectedTextUrl3Message:
        case kSelectedTextUrl4Message:
        case kSelectedTextUrl5Message:
        case kSelectedTextUrl6Message:
        case kSelectedTextUrl7Message:
            return !SelectedFocusedText().empty();
        case kPriorityHighestMessage:
        case kPriorityHighMessage:
        case kPriorityNormalMessage:
        case kPriorityLowMessage:
        case kPriorityLowestMessage:
            return has_message;
        default:
            return true;
    }
}

void HaikuMainWindow::SetCurrentMailbox(std::string mailbox_id, bool activate_window) {
    current_mailbox_id_ = std::move(mailbox_id);
    PopulateMessagesForCurrentMailbox();
    PopulatePreviewNow();
    UpdateSearchBarState();
    UpdateCommandState();
    if (activate_window) {
        Activate(true);
    }
}

bool HaikuMainWindow::SelectRelativeMessage(int delta, bool open_window) {
    if (message_list_ == nullptr || message_list_->CountRows() == 0 || delta == 0) {
        return false;
    }
    int32 current_index = -1;
    if (const auto selected_message_id = FirstSelectedMessageId()) {
        for (int32 index = 0; const auto* row = dynamic_cast<const TocMessageRow*>(message_list_->RowAt(index)); ++index) {
            if (row->message_id == *selected_message_id) {
                current_index = index;
                break;
            }
        }
    }
    if (current_index < 0) {
        current_index = 0;
    }
    const int32 target_index = std::clamp(current_index + delta, 0, message_list_->CountRows() - 1);
    if (target_index == current_index && !open_window) {
        return false;
    }
    auto* row = dynamic_cast<TocMessageRow*>(message_list_->RowAt(target_index));
    if (row == nullptr) {
        return false;
    }
    message_list_->DeselectAll();
    message_list_->SetFocusRow(row, true);
    current_message_id_ = row->message_id;
    PopulatePreviewNow();
    if (open_window) {
        HandleOpenSelectedMessage();
    }
    return true;
}

bool HaikuMainWindow::SelectMessage(std::string message_id, bool activate_window) {
    current_message_id_ = std::move(message_id);
    for (int32 index = 0; const auto* row = dynamic_cast<const TocMessageRow*>(message_list_->RowAt(index)); ++index) {
        if (row->message_id == current_message_id_) {
            if (auto* mutable_row = dynamic_cast<TocMessageRow*>(message_list_->RowAt(index))) {
                message_list_->DeselectAll();
                message_list_->SetFocusRow(mutable_row, true);
                PopulatePreviewNow();
                UpdateCommandState();
                if (activate_window) {
                    Activate(true);
                }
                return true;
            }
        }
    }
    PopulatePreviewNow();
    UpdateCommandState();
    if (activate_window) {
        Activate(true);
    }
    return false;
}

void HaikuMainWindow::SchedulePreviewRead() {
    CancelPreviewRead();
    if (!gui_preferences_.show_preview_pane || !gui_preferences_.mark_previewed_read ||
        gui_preferences_.preview_read_seconds <= 0) {
        return;
    }
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || !detail->unread || detail->mailbox_id == "drafts" || !detail->download_complete) {
        return;
    }
    preview_read_runner_ = std::make_unique<BMessageRunner>(BMessenger(this),
                                                            new BMessage(kPreviewReadTickMessage),
                                                            gui_preferences_.preview_read_seconds * 1000000LL,
                                                            1);
}

void HaikuMainWindow::SchedulePreviewRefresh() {
    CancelPreviewRefresh();
    preview_refresh_runner_ = std::make_unique<BMessageRunner>(BMessenger(this),
                                                               new BMessage(kPreviewRefreshTickMessage),
                                                               150000LL,
                                                               1);
}

void HaikuMainWindow::MarkSelectedMessageReadFromPreview() {
    preview_read_runner_.reset();
    if (!gui_preferences_.mark_previewed_read || current_message_id_.empty()) {
        return;
    }
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || !detail->unread || detail->mailbox_id == "drafts") {
        return;
    }
    if (!shell_host_.SetLegacyStatusForMessages(detail->mailbox_id, {detail->id}, LegacyMessageStatus::kRead)) {
        return;
    }
    SetStatusMessage("Marked previewed message as read.");
}

void HaikuMainWindow::CancelPreviewRead() {
    preview_read_runner_.reset();
}

void HaikuMainWindow::CancelPreviewRefresh() {
    preview_refresh_runner_.reset();
}

void HaikuMainWindow::SchedulePreviewFetch(std::string mailbox_id, std::string message_id) {
    preview_fetch_attempt_mailbox_id_ = std::move(mailbox_id);
    preview_fetch_attempt_message_id_ = std::move(message_id);
    BMessage fetch_message(kAutoFetchPreviewMessage);
    fetch_message.AddString("mailbox_id", preview_fetch_attempt_mailbox_id_.c_str());
    fetch_message.AddString("message_id", preview_fetch_attempt_message_id_.c_str());
    PostMessage(&fetch_message);
}

std::string HaikuMainWindow::SelectedPreviewText() const {
    if (preview_web_view_ != nullptr && !preview_web_view_->IsHidden()) {
        return TrimWhitespace(preview_web_view_->SelectedText());
    }
    return SelectedTextFromView(preview_text_);
}

std::string HaikuMainWindow::SelectedFocusedText() const {
    BView* focus = CurrentFocus();
    if (focus == nullptr) {
        return {};
    }
    if (preview_web_view_ != nullptr &&
        (focus == preview_web_view_ || IsFocusWithin(preview_container_))) {
        if (const std::string selected_preview = SelectedPreviewText(); !selected_preview.empty()) {
            return selected_preview;
        }
    }
    if (auto* text_view = dynamic_cast<BTextView*>(focus)) {
        return SelectedTextFromView(text_view);
    }
    if (auto* text_control = dynamic_cast<BTextControl*>(focus)) {
        return SelectedTextFromView(text_control->TextView());
    }
    return {};
}

void HaikuMainWindow::RefreshSelectedTextUrlActions() {
    selected_text_url_actions_ = SelectedTextUrlActionsFromSettings(shell_host_.Settings());
    for (auto* item : selected_text_url_menu_items_) {
        if (item != nullptr && edit_menu_ != nullptr) {
            edit_menu_->RemoveItem(item);
        }
        if (item != nullptr && item->Message() != nullptr) {
            const auto it = command_menu_items_.find(item->Message()->what);
            if (it != command_menu_items_.end()) {
                auto& items = it->second;
                items.erase(std::remove(items.begin(), items.end(), item), items.end());
            }
        }
        delete item;
    }
    selected_text_url_menu_items_.clear();

    if (edit_menu_ == nullptr || selected_text_url_actions_.empty()) {
        return;
    }

    for (const auto& action : selected_text_url_actions_) {
        const uint32 command = SelectedTextUrlCommandForSlot(action.slot);
        if (command == 0) {
            continue;
        }
        std::string label = action.label;
        if (const std::string accelerator = hermes::SelectedTextUrlAcceleratorLabelForSlot(action.slot);
            !accelerator.empty()) {
            label += '\t';
            label += accelerator;
        }
        auto* item = new BMenuItem(label.c_str(), new BMessage(command));
        command_menu_items_[command].push_back(item);
        selected_text_url_menu_items_.push_back(item);
        edit_menu_->AddItem(item);
    }
}

void HaikuMainWindow::SaveWindowLayoutState() {
    PersistGuiPreferences();
}

void HaikuMainWindow::ShowMessageContextMenu(BPoint where) {
    BPopUpMenu menu("message-context", false, false);
    const SelectedCommandState state = ComputeSelectedCommandState();
    const auto mail_transfer = shell_host_.MailTransfer();
    auto apply_checked_state = [&state](BMenuItem* item) {
        if (item == nullptr || item->Message() == nullptr || !state.has_selection) {
            return;
        }
        switch (item->Message()->what) {
            case kMarkReadMessage:
                item->SetMarked(state.all_read);
                break;
            case kMarkUnreadMessage:
                item->SetMarked(state.all_unread);
                break;
            case kSetLegacyStatusMessage: {
                int32 status = -1;
                if (state.unanimous_status &&
                    item->Message()->FindInt32("legacy_status", &status) == B_OK) {
                    item->SetMarked(status == static_cast<int32>(*state.unanimous_status));
                }
                break;
            }
            case kSetLabelMessage: {
                int32 label_index = -1;
                if (state.unanimous_label &&
                    item->Message()->FindInt32("label_index", &label_index) == B_OK) {
                    item->SetMarked(label_index == *state.unanimous_label);
                }
                break;
            }
            case kPriorityHighestMessage:
            case kPriorityHighMessage:
            case kPriorityNormalMessage:
            case kPriorityLowMessage:
            case kPriorityLowestMessage: {
                if (!state.unanimous_priority) {
                    break;
                }
                const std::optional<int> expected = [&]() -> std::optional<int> {
                    switch (item->Message()->what) {
                        case kPriorityHighestMessage:
                            return static_cast<int>(ComposePriority::kHighest);
                        case kPriorityHighMessage:
                            return static_cast<int>(ComposePriority::kHigh);
                        case kPriorityNormalMessage:
                            return static_cast<int>(ComposePriority::kNormal);
                        case kPriorityLowMessage:
                            return static_cast<int>(ComposePriority::kLow);
                        case kPriorityLowestMessage:
                            return static_cast<int>(ComposePriority::kLowest);
                        default:
                            return std::nullopt;
                    }
                }();
                if (expected) {
                    item->SetMarked(*expected == *state.unanimous_priority);
                }
                break;
            }
            case kServerLeaveMessage:
            case kServerFetchMessage:
            case kServerDeleteMessage:
            case kServerFetchDeleteMessage: {
                if (!state.unanimous_pop_status) {
                    break;
                }
                const std::optional<PopServerStatus> expected = [&]() -> std::optional<PopServerStatus> {
                    switch (item->Message()->what) {
                        case kServerLeaveMessage:
                            return PopServerStatus::kLeave;
                        case kServerFetchMessage:
                            return PopServerStatus::kFetch;
                        case kServerDeleteMessage:
                            return PopServerStatus::kDelete;
                        case kServerFetchDeleteMessage:
                            return PopServerStatus::kFetchDelete;
                        default:
                            return std::nullopt;
                    }
                }();
                if (expected) {
                    item->SetMarked(*expected == *state.unanimous_pop_status);
                }
                break;
            }
            default:
                break;
        }
    };
    auto add_item = [this, &menu, &apply_checked_state](const char* label, uint32 command) -> BMenuItem* {
        auto* item = new BMenuItem(label, new BMessage(command));
        item->SetEnabled(IsCommandEnabled(command));
        apply_checked_state(item);
        menu.AddItem(item);
        return item;
    };
    auto add_enabled_subitem =
        [this, &apply_checked_state](BMenu* submenu, const char* label, uint32 command) -> BMenuItem* {
        auto* item = new BMenuItem(label, new BMessage(command));
        item->SetEnabled(IsCommandEnabled(command));
        apply_checked_state(item);
        submenu->AddItem(item);
        return item;
    };
    auto add_status_menu = [this, &add_enabled_subitem, &apply_checked_state](BMenu& parent) {
        auto* submenu = new BMenu("Status");
        add_enabled_subitem(submenu, "Mark Read", kMarkReadMessage);
        add_enabled_subitem(submenu, "Mark Unread", kMarkUnreadMessage);
        add_enabled_subitem(submenu, "Toggle Read Status", kToggleStatusMessage);
        submenu->AddSeparatorItem();
        const auto add_status =
            [&add_enabled_subitem, &apply_checked_state, submenu](const char* label, LegacyMessageStatus status) {
            auto* item = add_enabled_subitem(submenu, label, kSetLegacyStatusMessage);
            item->Message()->AddInt32("legacy_status", static_cast<int32>(status));
            apply_checked_state(item);
        };
        add_status("Replied", LegacyMessageStatus::kReplied);
        add_status("Forwarded", LegacyMessageStatus::kForwarded);
        add_status("Redirected", LegacyMessageStatus::kRedirected);
        add_status("Unsendable", LegacyMessageStatus::kUnsendable);
        add_status("Recovered", LegacyMessageStatus::kRecovered);
        add_status("Sendable", LegacyMessageStatus::kSendable);
        add_status("Queued", LegacyMessageStatus::kQueued);
        add_status("Time Queued", LegacyMessageStatus::kTimeQueued);
        add_status("Sent", LegacyMessageStatus::kSent);
        add_status("Unsent", LegacyMessageStatus::kUnsent);
        parent.AddItem(submenu);
    };
    auto add_label_menu = [this, &add_enabled_subitem, &apply_checked_state](BMenu& parent) {
        auto* submenu = new BMenu("Label");
        auto add_label = [this, &add_enabled_subitem, &apply_checked_state, submenu](std::string label, int index) {
            auto* item = add_enabled_subitem(submenu, label.c_str(), kSetLabelMessage);
            item->Message()->AddInt32("label_index", index);
            apply_checked_state(item);
        };
        add_label("No Label", 0);
        for (int index = 1; index <= 7; ++index) {
            add_label(MailboxLabelName(mailbox_ui_, index), index);
        }
        parent.AddItem(submenu);
    };
    auto add_priority_menu = [&add_enabled_subitem](BMenu& parent) {
        auto* submenu = new BMenu("Priority");
        add_enabled_subitem(submenu, "Highest", kPriorityHighestMessage);
        add_enabled_subitem(submenu, "High", kPriorityHighMessage);
        add_enabled_subitem(submenu, "Normal", kPriorityNormalMessage);
        add_enabled_subitem(submenu, "Low", kPriorityLowMessage);
        add_enabled_subitem(submenu, "Lowest", kPriorityLowestMessage);
        parent.AddItem(submenu);
    };
    auto add_server_status_menu = [&add_enabled_subitem](BMenu& parent) {
        auto* submenu = new BMenu("Server Status");
        add_enabled_subitem(submenu, "Leave", kServerLeaveMessage);
        add_enabled_subitem(submenu, "Fetch", kServerFetchMessage);
        add_enabled_subitem(submenu, "Delete", kServerDeleteMessage);
        add_enabled_subitem(submenu, "Fetch + Delete", kServerFetchDeleteMessage);
        parent.AddItem(submenu);
    };
    auto add_imap_menu = [&add_enabled_subitem](BMenu& parent) {
        auto* submenu = new BMenu("IMAP");
        add_enabled_subitem(submenu, "Fetch Full", kFetchFullMessageMessage);
        add_enabled_subitem(submenu, "Fetch Default", kFetchDefaultMessageMessage);
        submenu->AddSeparatorItem();
        add_enabled_subitem(submenu, "Redownload Full", kImapRedownloadFullMessage);
        add_enabled_subitem(submenu, "Redownload Default", kImapRedownloadDefaultMessage);
        submenu->AddSeparatorItem();
        add_enabled_subitem(submenu, "Clear Cached", kImapClearCachedMessage);
        parent.AddItem(submenu);
    };
    add_item("Open", kOpenMessageMessage);
    add_item(ReplyMenuLabel(false), kReplyMessage);
    add_item(ReplyMenuLabel(true), kReplyAllMessage);
    add_item("Forward", kForwardMessage);
    add_item("Redirect", kRedirectMessage);
    add_item("Send Again", kSendAgainMessage);
    add_item(SendImmediateMenuLabel(mail_transfer.immediate_send), kSendImmediatelyMessage);
    add_item("Change Queueing" B_UTF8_ELLIPSIS, kChangeQueueingMessage);
    menu.AddSeparatorItem();
    add_item("Delete", kDeleteMessageMessage);
    add_item("Undelete", kUndeleteMessageMessage);
    add_item("Purge/Expunge", kPurgeMailboxMessage);
    menu.AddSeparatorItem();
    add_status_menu(menu);
    add_label_menu(menu);
    add_priority_menu(menu);
    add_server_status_menu(menu);
    add_imap_menu(menu);
    menu.AddSeparatorItem();
    add_item("Move", kMoveMessageMessage);
    add_item("Copy", kCopyMessageMessage);
    menu.AddSeparatorItem();
    add_item("Junk", kMarkJunkMessage);
    add_item("Not Junk", kMarkNotJunkMessage);
    add_item("Recheck Junk", kRecheckJunkMessage);
    menu.AddSeparatorItem();
    add_item("Filter Messages", kFilterMessagesMessage);
    add_item("Make Filter", kMakeFilterMessage);
    menu.AddSeparatorItem();
    add_item("Find Messages" B_UTF8_ELLIPSIS, kFindMessagesMessage);
    add_item("Make Nickname", kMakeNicknameMessage);
    menu.AddSeparatorItem();
    add_item("Print Preview", kPrintPreviewMessage);
    add_item("Print One", kPrintDirectMessage);
    menu.SetTargetForItems(this);
    BPoint screen_where = where;
    message_list_->ConvertToScreen(&screen_where);
    if (BMenuItem* item = menu.Go(screen_where)) {
        item->Invoke();
    }
}

void HaikuMainWindow::ShowPreviewContextMenu(BPoint where) {
    BPopUpMenu menu("preview-context", false, false);
    const SelectedCommandState state = ComputeSelectedCommandState();
    const auto mail_transfer = shell_host_.MailTransfer();
    auto apply_checked_state = [&state](BMenuItem* item) {
        if (item == nullptr || item->Message() == nullptr || !state.has_selection) {
            return;
        }
        switch (item->Message()->what) {
            case kMarkReadMessage:
                item->SetMarked(state.all_read);
                break;
            case kMarkUnreadMessage:
                item->SetMarked(state.all_unread);
                break;
            case kSetLegacyStatusMessage: {
                int32 status = -1;
                if (state.unanimous_status &&
                    item->Message()->FindInt32("legacy_status", &status) == B_OK) {
                    item->SetMarked(status == static_cast<int32>(*state.unanimous_status));
                }
                break;
            }
            case kSetLabelMessage: {
                int32 label_index = -1;
                if (state.unanimous_label &&
                    item->Message()->FindInt32("label_index", &label_index) == B_OK) {
                    item->SetMarked(label_index == *state.unanimous_label);
                }
                break;
            }
            case kPriorityHighestMessage:
            case kPriorityHighMessage:
            case kPriorityNormalMessage:
            case kPriorityLowMessage:
            case kPriorityLowestMessage: {
                if (!state.unanimous_priority) {
                    break;
                }
                const std::optional<int> expected = [&]() -> std::optional<int> {
                    switch (item->Message()->what) {
                        case kPriorityHighestMessage:
                            return static_cast<int>(ComposePriority::kHighest);
                        case kPriorityHighMessage:
                            return static_cast<int>(ComposePriority::kHigh);
                        case kPriorityNormalMessage:
                            return static_cast<int>(ComposePriority::kNormal);
                        case kPriorityLowMessage:
                            return static_cast<int>(ComposePriority::kLow);
                        case kPriorityLowestMessage:
                            return static_cast<int>(ComposePriority::kLowest);
                        default:
                            return std::nullopt;
                    }
                }();
                if (expected) {
                    item->SetMarked(*expected == *state.unanimous_priority);
                }
                break;
            }
            case kServerLeaveMessage:
            case kServerFetchMessage:
            case kServerDeleteMessage:
            case kServerFetchDeleteMessage: {
                if (!state.unanimous_pop_status) {
                    break;
                }
                const std::optional<PopServerStatus> expected = [&]() -> std::optional<PopServerStatus> {
                    switch (item->Message()->what) {
                        case kServerLeaveMessage:
                            return PopServerStatus::kLeave;
                        case kServerFetchMessage:
                            return PopServerStatus::kFetch;
                        case kServerDeleteMessage:
                            return PopServerStatus::kDelete;
                        case kServerFetchDeleteMessage:
                            return PopServerStatus::kFetchDelete;
                        default:
                            return std::nullopt;
                    }
                }();
                if (expected) {
                    item->SetMarked(*expected == *state.unanimous_pop_status);
                }
                break;
            }
            default:
                break;
        }
    };
    auto add_item = [this, &menu, &apply_checked_state](const char* label, uint32 command) -> BMenuItem* {
        auto* item = new BMenuItem(label, new BMessage(command));
        item->SetEnabled(IsCommandEnabled(command));
        apply_checked_state(item);
        menu.AddItem(item);
        return item;
    };
    auto add_enabled_subitem =
        [this, &apply_checked_state](BMenu* submenu, const char* label, uint32 command) -> BMenuItem* {
        auto* item = new BMenuItem(label, new BMessage(command));
        item->SetEnabled(IsCommandEnabled(command));
        apply_checked_state(item);
        submenu->AddItem(item);
        return item;
    };
    auto add_status_menu = [this, &add_enabled_subitem, &apply_checked_state](BMenu& parent) {
        auto* submenu = new BMenu("Status");
        add_enabled_subitem(submenu, "Mark Read", kMarkReadMessage);
        add_enabled_subitem(submenu, "Mark Unread", kMarkUnreadMessage);
        add_enabled_subitem(submenu, "Toggle Read Status", kToggleStatusMessage);
        submenu->AddSeparatorItem();
        const auto add_status =
            [&add_enabled_subitem, &apply_checked_state, submenu](const char* label, LegacyMessageStatus status) {
            auto* item = add_enabled_subitem(submenu, label, kSetLegacyStatusMessage);
            item->Message()->AddInt32("legacy_status", static_cast<int32>(status));
            apply_checked_state(item);
        };
        add_status("Replied", LegacyMessageStatus::kReplied);
        add_status("Forwarded", LegacyMessageStatus::kForwarded);
        add_status("Redirected", LegacyMessageStatus::kRedirected);
        add_status("Unsendable", LegacyMessageStatus::kUnsendable);
        add_status("Recovered", LegacyMessageStatus::kRecovered);
        add_status("Sendable", LegacyMessageStatus::kSendable);
        add_status("Queued", LegacyMessageStatus::kQueued);
        add_status("Time Queued", LegacyMessageStatus::kTimeQueued);
        add_status("Sent", LegacyMessageStatus::kSent);
        add_status("Unsent", LegacyMessageStatus::kUnsent);
        parent.AddItem(submenu);
    };
    auto add_label_menu = [this, &add_enabled_subitem, &apply_checked_state](BMenu& parent) {
        auto* submenu = new BMenu("Label");
        auto add_label = [this, &add_enabled_subitem, &apply_checked_state, submenu](std::string label, int index) {
            auto* item = add_enabled_subitem(submenu, label.c_str(), kSetLabelMessage);
            item->Message()->AddInt32("label_index", index);
            apply_checked_state(item);
        };
        add_label("No Label", 0);
        for (int index = 1; index <= 7; ++index) {
            add_label(MailboxLabelName(mailbox_ui_, index), index);
        }
        parent.AddItem(submenu);
    };
    auto add_priority_menu = [&add_enabled_subitem](BMenu& parent) {
        auto* submenu = new BMenu("Priority");
        add_enabled_subitem(submenu, "Highest", kPriorityHighestMessage);
        add_enabled_subitem(submenu, "High", kPriorityHighMessage);
        add_enabled_subitem(submenu, "Normal", kPriorityNormalMessage);
        add_enabled_subitem(submenu, "Low", kPriorityLowMessage);
        add_enabled_subitem(submenu, "Lowest", kPriorityLowestMessage);
        parent.AddItem(submenu);
    };
    auto add_server_status_menu = [&add_enabled_subitem](BMenu& parent) {
        auto* submenu = new BMenu("Server Status");
        add_enabled_subitem(submenu, "Leave", kServerLeaveMessage);
        add_enabled_subitem(submenu, "Fetch", kServerFetchMessage);
        add_enabled_subitem(submenu, "Delete", kServerDeleteMessage);
        add_enabled_subitem(submenu, "Fetch + Delete", kServerFetchDeleteMessage);
        parent.AddItem(submenu);
    };
    auto add_imap_menu = [&add_enabled_subitem](BMenu& parent) {
        auto* submenu = new BMenu("IMAP");
        add_enabled_subitem(submenu, "Fetch Full", kFetchFullMessageMessage);
        add_enabled_subitem(submenu, "Fetch Default", kFetchDefaultMessageMessage);
        submenu->AddSeparatorItem();
        add_enabled_subitem(submenu, "Redownload Full", kImapRedownloadFullMessage);
        add_enabled_subitem(submenu, "Redownload Default", kImapRedownloadDefaultMessage);
        submenu->AddSeparatorItem();
        add_enabled_subitem(submenu, "Clear Cached", kImapClearCachedMessage);
        parent.AddItem(submenu);
    };

    add_item("Open", kOpenMessageMessage);
    add_item(ReplyMenuLabel(false), kReplyMessage);
    add_item(ReplyMenuLabel(true), kReplyAllMessage);
    add_item("Forward", kForwardMessage);
    add_item("Redirect", kRedirectMessage);
    add_item("Send Again", kSendAgainMessage);
    add_item(SendImmediateMenuLabel(mail_transfer.immediate_send), kSendImmediatelyMessage);
    add_item("Change Queueing" B_UTF8_ELLIPSIS, kChangeQueueingMessage);
    menu.AddSeparatorItem();

    if (const std::string selected_text = SelectedPreviewText(); !selected_text.empty()) {
        const auto mailbox = shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
        if (mailbox && mailbox->protocol == MailboxProtocol::kImap) {
            std::vector<MailboxRecord> matches;
            const std::string needle = ToLower(selected_text);
            const bool copy_instead_of_move = (modifiers() & B_SHIFT_KEY) != 0;
            if (auto recent_mailboxes =
                    RecentMailboxEntries(std::string_view(mailbox->account_id), true, current_mailbox_id_);
                !recent_mailboxes.empty()) {
                auto* recent_menu = new BMenu("Recent");
                for (const auto& recent_mailbox : recent_mailboxes) {
                    auto* transfer_message =
                        new BMessage(copy_instead_of_move ? kPerformCopyMessage : kPerformMoveMessage);
                    transfer_message->AddString("destination_mailbox_id", recent_mailbox.id.c_str());
                    recent_menu->AddItem(
                        new BMenuItem(recent_mailbox.display_name.c_str(), transfer_message));
                }
                menu.AddItem(recent_menu);
                menu.AddSeparatorItem();
            }
            for (const auto& candidate : shell_host_.Mailboxes().ListMailboxes()) {
                if (candidate.protocol != MailboxProtocol::kImap || candidate.account_id != mailbox->account_id ||
                    candidate.id == current_mailbox_id_) {
                    continue;
                }
                const std::string haystack = ToLower(candidate.display_name);
                if (haystack.find(needle) != std::string::npos || needle.find(haystack) != std::string::npos) {
                    matches.push_back(candidate);
                }
            }
            if (!matches.empty()) {
                auto* transfer_menu = new BMenu(copy_instead_of_move ? "Copy To" : "Transfer To");
                for (const auto& match : matches) {
                    auto* transfer_message =
                        new BMessage(copy_instead_of_move ? kPerformCopyMessage : kPerformMoveMessage);
                    transfer_message->AddString("destination_mailbox_id", match.id.c_str());
                    transfer_menu->AddItem(new BMenuItem(match.display_name.c_str(), transfer_message));
                }
                menu.AddItem(transfer_menu);
                menu.AddSeparatorItem();
            }
        }
    }

    add_item("Delete", kDeleteMessageMessage);
    add_item("Undelete", kUndeleteMessageMessage);
    add_item("Purge/Expunge", kPurgeMailboxMessage);
    menu.AddSeparatorItem();
    add_status_menu(menu);
    add_label_menu(menu);
    add_priority_menu(menu);
    add_server_status_menu(menu);
    add_imap_menu(menu);
    menu.AddSeparatorItem();
    add_item("Move", kMoveMessageMessage);
    add_item("Copy", kCopyMessageMessage);
    menu.AddSeparatorItem();
    add_item("Junk", kMarkJunkMessage);
    add_item("Not Junk", kMarkNotJunkMessage);
    add_item("Recheck Junk", kRecheckJunkMessage);
    menu.AddSeparatorItem();
    add_item("Filter Messages", kFilterMessagesMessage);
    add_item("Make Filter", kMakeFilterMessage);
    menu.AddSeparatorItem();
    add_item("Find Messages" B_UTF8_ELLIPSIS, kFindMessagesMessage);
    add_item("Make Nickname", kMakeNicknameMessage);
    menu.AddSeparatorItem();
    add_item("Print Preview", kPrintPreviewMessage);
    add_item("Print One", kPrintDirectMessage);

    menu.SetTargetForItems(this);
    BPoint screen_where = where;
    if (preview_web_view_ != nullptr && !preview_web_view_->IsHidden()) {
        preview_web_view_->ConvertToScreen(&screen_where);
    } else if (preview_text_ != nullptr) {
        preview_text_->ConvertToScreen(&screen_where);
    }
    if (BMenuItem* item = menu.Go(screen_where)) {
        item->Invoke();
    }
}

void HaikuMainWindow::ShowAttachmentContextMenu(BPoint where) {
    BPopUpMenu menu("attachment-context", false, false);
    menu.AddItem(new BMenuItem("Open", new BMessage(kOpenAttachmentMessage)));
    menu.AddItem(new BMenuItem("Save", new BMessage(kSaveAttachmentMessage)));
    menu.AddItem(new BMenuItem("Save All", new BMessage(kSaveAllAttachmentsMessage)));
    menu.AddSeparatorItem();
    menu.AddItem(new BMenuItem("Fetch", new BMessage(kFetchAttachmentMessage)));
    menu.SetTargetForItems(this);
    BPoint screen_where = where;
    attachment_list_->ConvertToScreen(&screen_where);
    if (BMenuItem* item = menu.Go(screen_where)) {
        item->Invoke();
    }
}

void HaikuMainWindow::HandleOpenSelectedMessage() {
    const auto selected_message_id = FirstSelectedMessageId();
    if (!selected_message_id) {
        return;
    }
    if (shell_host_.OpenMessageWindow(current_mailbox_id_, *selected_message_id)) {
        SetStatusMessage("Opened the selected message.");
    } else {
        SetStatusMessage("Unable to open the selected message.");
    }
}

void HaikuMainWindow::HandleOpenSelectedAttachment() {
    const int32 selection = attachment_list_->CurrentSelection();
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || selection < 0 || static_cast<std::size_t>(selection) >= attachment_indices_.size()) {
        return;
    }

    const std::size_t attachment_index = attachment_indices_[static_cast<std::size_t>(selection)];
    const auto path = shell_host_.AttachmentPath(detail->mailbox_id, detail->id, attachment_index);
    if (path && std::filesystem::exists(*path) && LaunchPath(*path)) {
        shell_host_.RecordAttachmentLaunch(detail->attachments[attachment_index].name,
                                           *path,
                                           detail->mailbox_id + ":" + detail->id);
        SetStatusMessage("Opened selected attachment.");
        return;
    }

    const auto& attachment = detail->attachments[attachment_index];
    if (attachment.omitted || !attachment.download_complete) {
        if (BAlert("attachment-fetch-alert",
                   "This attachment is not downloaded yet. Fetch it now?",
                   "Cancel",
                   "Fetch")
                ->Go() == 1) {
            HandleFetchSelectedAttachment();
        }
        return;
    }

    SetStatusMessage("Unable to open the selected attachment.");
    SelectUtilityTab(1);
}

void HaikuMainWindow::HandleSaveSelectedAttachment() {
    const int32 selection = attachment_list_->CurrentSelection();
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || selection < 0 || static_cast<std::size_t>(selection) >= attachment_indices_.size()) {
        return;
    }

    const std::size_t attachment_index = attachment_indices_[static_cast<std::size_t>(selection)];
    const auto& attachment = detail->attachments[attachment_index];
    const std::filesystem::path destination =
        DefaultAttachmentSaveRoot() /
        (attachment.name.empty() ? ("attachment-" + std::to_string(attachment_index)) : attachment.name);

    if (!shell_host_.SaveAttachment(detail->mailbox_id, detail->id, attachment_index, destination)) {
        if (attachment.omitted || !attachment.download_complete) {
            if (BAlert("attachment-fetch-save-alert",
                       "This attachment is not downloaded yet. Fetch it now?",
                       "Cancel",
                       "Fetch")
                    ->Go() == 1) {
                HandleFetchSelectedAttachment();
            }
            return;
        }
        SetStatusMessage("Unable to save the selected attachment.");
        SelectUtilityTab(1);
        return;
    }

    SetStatusMessage("Attachment saved to " + destination.string());
}

void HaikuMainWindow::HandleSaveAllAttachments() {
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || detail->attachments.empty()) {
        return;
    }

    const bool requires_fetch =
        std::any_of(detail->attachments.begin(), detail->attachments.end(), [](const AttachmentSummary& attachment) {
            return attachment.omitted || !attachment.download_complete;
        });
    if (requires_fetch) {
        if (BAlert("fetch-full-message-alert",
                   "Some attachments still need to be downloaded. Fetch the full message now?",
                   "Cancel",
                   "Fetch")
                ->Go() == 1) {
            HandleFetchSelectedMessage();
        }
        return;
    }

    const std::filesystem::path destination = DefaultAttachmentSaveRoot() / detail->id;
    if (!shell_host_.SaveAllAttachments(detail->mailbox_id, detail->id, destination)) {
        SetStatusMessage("Unable to save all attachments.");
        SelectUtilityTab(1);
        return;
    }

    SetStatusMessage("Saved all attachments to " + destination.string());
}

void HaikuMainWindow::HandleFetchSelectedAttachment() {
    const int32 selection = attachment_list_->CurrentSelection();
    const auto detail = shell_host_.WorkspaceMessageDetail(current_message_id_);
    if (!detail || selection < 0 || static_cast<std::size_t>(selection) >= attachment_indices_.size()) {
        return;
    }

    const std::size_t attachment_index = attachment_indices_[static_cast<std::size_t>(selection)];
    const bool success =
        shell_host_.FetchAttachment(detail->mailbox_id, detail->id, attachment_index) && shell_host_.CheckMail();
    PopulateTaskStatus();
    SetStatusMessage(success ? "Attachment fetch queued." : "Unable to fetch the selected attachment.");
}

void HaikuMainWindow::HandleFetchSelectedMessage(bool use_default_fetch) {
    if ((modifiers() & B_SHIFT_KEY) != 0) {
        HandleRedownloadSelectedMessages(!use_default_fetch);
        return;
    }
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    bool success = true;
    for (const auto& message_id : selected_ids) {
        const auto detail = shell_host_.WorkspaceMessageDetail(message_id);
        if (!detail) {
            success = false;
            continue;
        }
        success = (use_default_fetch
                       ? shell_host_.FetchDefaultImapMessages(detail->mailbox_id, {detail->id})
                       : shell_host_.FetchFullMessage(detail->mailbox_id, detail->id)) &&
                  success;
    }
    if (success) {
        success = shell_host_.CheckMail();
    }
    PopulateTaskStatus();
    SetStatusMessage(success ? (use_default_fetch ? "Default IMAP fetch queued." : "Full message fetch queued.")
                             : (use_default_fetch ? "Unable to fetch the default IMAP message body."
                                                  : "Unable to fetch the full message."));
}

void HaikuMainWindow::HandleMoveOrCopySelectedMessage(bool copy) {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }

    const auto source_mailbox = shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
    if (!source_mailbox || source_mailbox->protocol != MailboxProtocol::kImap) {
        ShowInfoAlert(copy ? "copy-message-alert" : "move-message-alert",
                      "Message copy and move are only available for IMAP mailboxes.");
        return;
    }

    BPopUpMenu menu(copy ? "copy-message-menu" : "move-message-menu", false, false);
    if (auto recent_mailboxes =
            RecentMailboxEntries(std::string_view(source_mailbox->account_id), true, current_mailbox_id_);
        !recent_mailboxes.empty()) {
        auto* recent_menu = new BMenu("Recent");
        for (const auto& mailbox : recent_mailboxes) {
            auto* item_message = new BMessage(copy ? kPerformCopyMessage : kPerformMoveMessage);
            item_message->AddString("destination_mailbox_id", mailbox.id.c_str());
            recent_menu->AddItem(new BMenuItem(mailbox.display_name.c_str(), item_message));
        }
        menu.AddItem(recent_menu);
        menu.AddSeparatorItem();
    }
    for (const auto& mailbox : shell_host_.Mailboxes().ListMailboxes()) {
        if (mailbox.protocol != MailboxProtocol::kImap || mailbox.account_id != source_mailbox->account_id ||
            mailbox.id == current_mailbox_id_) {
            continue;
        }
        auto* item_message = new BMessage(copy ? kPerformCopyMessage : kPerformMoveMessage);
        item_message->AddString("destination_mailbox_id", mailbox.id.c_str());
        menu.AddItem(new BMenuItem(mailbox.display_name.c_str(), item_message));
    }
    if (menu.CountItems() == 0) {
        ShowInfoAlert(copy ? "copy-message-alert" : "move-message-alert",
                      "No destination IMAP mailboxes are available for this account.");
        return;
    }

    menu.SetTargetForItems(this);
    if (BMenuItem* item = menu.Go(ConvertToScreen(BPoint(220.0f, 220.0f)))) {
        item->Invoke();
    }
}

void HaikuMainWindow::HandleCreateMailbox() {
    std::string account_id;
    if (const auto mailbox = shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
        mailbox && mailbox->protocol == MailboxProtocol::kImap) {
        account_id = mailbox->account_id;
    } else {
        for (const auto& mailbox : shell_host_.Mailboxes().ListMailboxes()) {
            if (mailbox.protocol == MailboxProtocol::kImap) {
                account_id = mailbox.account_id;
                break;
            }
        }
    }

    if (account_id.empty()) {
        ShowInfoAlert("create-mailbox-alert", "No IMAP account is available for remote mailbox creation.");
        return;
    }

    BMessage payload(kCreateMailboxConfirmed);
    payload.AddString("account_id", account_id.c_str());
    auto* prompt =
        new TextPromptWindow("Create Remote Mailbox", "Remote mailbox name", "", BMessenger(this), payload);
    prompt->Show();
}

void HaikuMainWindow::HandleRenameMailbox() {
    const auto mailbox = shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap) {
        ShowInfoAlert("rename-mailbox-alert", "Select an IMAP mailbox to rename.");
        return;
    }

    BMessage payload(kRenameMailboxConfirmed);
    payload.AddString("mailbox_id", mailbox->id.c_str());
    auto* prompt = new TextPromptWindow("Rename Remote Mailbox",
                                        "Remote mailbox name",
                                        mailbox->remote_name.empty() ? mailbox->display_name : mailbox->remote_name,
                                        BMessenger(this),
                                        payload);
    prompt->Show();
}

void HaikuMainWindow::HandleDeleteMailbox() {
    const auto mailbox = shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
    if (!mailbox || mailbox->protocol != MailboxProtocol::kImap) {
        ShowInfoAlert("delete-mailbox-alert", "Select an IMAP mailbox to delete.");
        return;
    }

    if (BAlert("delete-mailbox-alert",
               ("Delete remote mailbox \"" + mailbox->display_name + "\"?").c_str(),
               "Cancel",
               "Delete")
            ->Go() != 1) {
        return;
    }

    const bool queued = shell_host_.DeleteRemoteMailbox(mailbox->id);
    PopulateTaskStatus();
    SetStatusMessage(queued ? "Remote mailbox deletion queued." :
                              "Unable to queue remote mailbox deletion.");
}

void HaikuMainWindow::HandleComposeResponse(int kind, std::string_view stationery_name) {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }

    const auto response_kind = static_cast<HaikuShellHost::MessageResponseKind>(kind);
    const bool multi_reply =
        (response_kind == HaikuShellHost::MessageResponseKind::kReply ||
         response_kind == HaikuShellHost::MessageResponseKind::kReplyAll) &&
        selected_ids.size() > 1;
    if (multi_reply && !mailbox_ui_.multiple_replies_for_multiple_selections &&
        selected_ids.size() > static_cast<std::size_t>(std::max(1, mailbox_ui_.multiple_reply_warn_threshold))) {
        const std::string prompt = "Open reply windows for " + std::to_string(selected_ids.size()) +
                                   " selected messages?";
        if (BAlert("multi-reply-warning", prompt.c_str(), "Cancel", "Reply")->Go() != 1) {
            SetStatusMessage("Reply command cancelled.");
            return;
        }
    }

    std::size_t opened = 0;
    const auto ids_to_open = multi_reply ? selected_ids : std::vector<std::string>{selected_ids.front()};
    for (const auto& message_id : ids_to_open) {
        const auto response = shell_host_.BuildResponseMessage(response_kind, current_mailbox_id_, message_id, stationery_name);
        if (!response) {
            continue;
        }
        if (shell_host_.OpenComposer(*response)) {
            ++opened;
        }
    }
    SetStatusMessage(opened > 0 ? "Opened compose window(s) from the selected message set."
                                : "Unable to prepare a response for the selected message.");
}

void HaikuMainWindow::HandleSendImmediately() {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    if (!shell_host_.ChangeQueueingForMessages(current_mailbox_id_, selected_ids, 0)) {
        SetStatusMessage("Unable to update queueing for the selected message.");
        return;
    }
    PopulateTaskStatus();
    const bool flush_queue = shell_host_.MailTransfer().immediate_send ^ ((modifiers() & B_SHIFT_KEY) != 0);
    if (!flush_queue) {
        SetStatusMessage("Queued the selected message set for delivery.");
        return;
    }
    const bool sent = shell_host_.SendQueued();
    PopulateTaskStatus();
    SetStatusMessage(sent ? "Queued message send started immediately."
                          : "Queued message was updated, but send did not complete cleanly.");
}

void HaikuMainWindow::HandleToggleSelectedStatus() {
    const auto selected_message_id = FirstSelectedMessageId();
    if (!selected_message_id) {
        return;
    }
    const auto detail = shell_host_.WorkspaceMessageDetail(*selected_message_id);
    if (!detail) {
        return;
    }
    HandleMarkSelectedMessageUnread(!detail->unread);
}

void HaikuMainWindow::HandleReplyShortcut(bool invert_reply_mapping) {
    const bool reply_to_all = shell_host_.ShellBehavior().reply_ctrl_r_to_all ^ invert_reply_mapping;
    HandleComposeResponse(static_cast<int>(reply_to_all ? HaikuShellHost::MessageResponseKind::kReplyAll
                                                        : HaikuShellHost::MessageResponseKind::kReply));
}

void HaikuMainWindow::HandlePreviousMessage() {
    const auto behavior = shell_host_.ShellBehavior();
    const int32 mods = modifiers();
    if ((mods & B_COMMAND_KEY) != 0 && !behavior.control_arrows) {
        return;
    }
    if ((mods & B_CONTROL_KEY) != 0 && !behavior.alt_arrows) {
        return;
    }
    if (SelectRelativeMessage(-1)) {
        SetStatusMessage("Selected the previous message.");
    }
}

void HaikuMainWindow::HandleNextMessage() {
    const auto behavior = shell_host_.ShellBehavior();
    const int32 mods = modifiers();
    if ((mods & B_COMMAND_KEY) != 0 && !behavior.control_arrows) {
        return;
    }
    if ((mods & B_CONTROL_KEY) != 0 && !behavior.alt_arrows) {
        return;
    }
    if (SelectRelativeMessage(1)) {
        SetStatusMessage("Selected the next message.");
    }
}

void HaikuMainWindow::HandleWorkOfflineToggle() {
    const bool offline = !shell_host_.ShellBehavior().offline;
    if (shell_host_.SetOfflineMode(offline)) {
        UpdateViewMenuMarks();
        UpdateCommandState();
        SetStatusMessage(offline ? "Work Offline enabled." : "Work Offline disabled.");
    } else {
        SetStatusMessage("Unable to update offline mode.");
    }
}

void HaikuMainWindow::HandleMailTransferOptions(bool sending_only) {
    auto* window = new MailTransferOptionsWindow(
        shell_host_.Accounts().Accounts(), shell_host_.MailTransfer(), sending_only, BMessenger(this));
    window->Show();
}

void HaikuMainWindow::HandleEmptyTrash() {
    std::string error_message;
    bool removed_any = false;
    for (const auto& mailbox : shell_host_.Mailboxes().ListMailboxes()) {
        const std::string lowered_id = ToLower(mailbox.id);
        const std::string lowered_name = ToLower(mailbox.display_name);
        if (lowered_id == "trash" || lowered_name == "trash") {
            removed_any = shell_host_.ClearMailboxContents(mailbox.id, &error_message) || removed_any;
        }
    }
    SetStatusMessage(removed_any ? "Emptied Trash."
                                 : (error_message.empty() ? "No local Trash mailbox could be emptied."
                                                          : error_message));
}

void HaikuMainWindow::HandleTrimJunk() {
    std::string error_message;
    bool removed_any = false;
    for (const auto& mailbox : shell_host_.Mailboxes().ListMailboxes()) {
        const std::string lowered_id = ToLower(mailbox.id);
        const std::string lowered_name = ToLower(mailbox.display_name);
        if (lowered_id == "junk" || lowered_name == "junk") {
            removed_any = shell_host_.ClearMailboxContents(mailbox.id, &error_message) || removed_any;
        }
    }
    SetStatusMessage(removed_any ? "Deleted old junk."
                                 : (error_message.empty() ? "No local Junk mailbox could be cleaned."
                                                          : error_message));
}

void HaikuMainWindow::HandleCompactMailboxes() {
    std::string error_message;
    if (shell_host_.CompactMailboxes(&error_message)) {
        PopulateTaskStatus();
        SetStatusMessage("Compacted local mailboxes.");
    } else {
        SetStatusMessage(error_message.empty() ? "Unable to compact mailboxes." : error_message);
    }
}

void HaikuMainWindow::HandleForgetPasswords() {
    if (BAlert("forget-passwords",
               "Forget all stored incoming, outgoing, and OAuth credentials?",
               "Cancel",
               "Forget")
            ->Go() != 1) {
        return;
    }
    std::string error_message;
    if (shell_host_.ForgetAllCredentialsAndTokens(&error_message)) {
        SetStatusMessage("Forgot all stored credentials and OAuth tokens.");
    } else {
        SetStatusMessage(error_message.empty() ? "Unable to forget stored credentials." : error_message);
    }
}

void HaikuMainWindow::HandleChangePasswordDialog() {
    auto* window = new PasswordPromptWindow(shell_host_.Accounts().Accounts(), BMessenger(this));
    window->Show();
}

void HaikuMainWindow::HandleOptionsDialog() {
    auto* window = new OptionsWindow(shell_host_.ShellBehavior(),
                                     shell_host_.MailTransfer(),
                                     shell_host_.MailboxUi(),
                                     gui_preferences_,
                                     BMessenger(this));
    window->Show();
}

void HaikuMainWindow::HandleChangeQueueingPrompt() {
    if (SelectedMessageIds().empty()) {
        return;
    }
    BMessage payload(kChangeQueueingConfirmedMessage);
    auto* prompt = new TextPromptWindow("Change Queueing",
                                        "Delay in seconds",
                                        "0",
                                        BMessenger(this),
                                        payload);
    prompt->Show();
}

void HaikuMainWindow::HandleChangeQueueing(int delay_seconds) {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    if (!shell_host_.ChangeQueueingForMessages(current_mailbox_id_, selected_ids, delay_seconds)) {
        SetStatusMessage("Unable to change queueing for the selected message set.");
        return;
    }
    SetStatusMessage(delay_seconds > 0 ? "Queued message timing updated."
                                       : "Queued message timing cleared.");
}

void HaikuMainWindow::HandleMarkSelectedMessageUnread(bool unread) {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    const bool success = shell_host_.SetLegacyStatusForMessages(
        current_mailbox_id_,
        selected_ids,
        unread ? LegacyMessageStatus::kUnread : LegacyMessageStatus::kRead);
    SetStatusMessage(unread ? "Marked message unread." : "Marked message read.");
    if (!success) {
        SetStatusMessage("Unable to update message status.");
    }
}

void HaikuMainWindow::HandleSetSelectedLegacyStatus(LegacyMessageStatus status) {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    if (!shell_host_.SetLegacyStatusForMessages(current_mailbox_id_, selected_ids, status)) {
        SetStatusMessage("Unable to update message status.");
        return;
    }
    SetStatusMessage("Updated the selected message status.");
}

void HaikuMainWindow::HandleSetSelectedLabel(int label_index) {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    if (!shell_host_.SetLabelForMessages(current_mailbox_id_, selected_ids, label_index)) {
        SetStatusMessage("Unable to update message labels.");
        return;
    }
    SetStatusMessage("Updated message labels.");
}

void HaikuMainWindow::HandleSetSelectedPopServerStatus(PopServerStatus status) {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    if (!shell_host_.SetPopServerStatusForMessages(current_mailbox_id_, selected_ids, status)) {
        SetStatusMessage("Unable to update POP server status for the selected messages.");
        return;
    }
    const auto mailbox = shell_host_.Mailboxes().GetMailbox(current_mailbox_id_);
    const PopServerStatus effective_status =
        mailbox ? EffectivePopServerStatus(*mailbox, status, mailbox_ui_.delete_fetched_junk) : status;
    SetStatusMessage("Set POP server status to " + PopServerStatusLabel(effective_status) + ".");
}

void HaikuMainWindow::HandleJunkAction(MailboxJunkAction action) {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    if (!shell_host_.ApplyJunkActionToMessages(current_mailbox_id_, selected_ids, action)) {
        SetStatusMessage("Unable to update junk state for the selected messages.");
        return;
    }
    switch (action) {
        case MailboxJunkAction::kJunk:
            SetStatusMessage("Marked the selected messages as junk.");
            break;
        case MailboxJunkAction::kNotJunk:
            SetStatusMessage("Marked the selected messages as not junk.");
            break;
        case MailboxJunkAction::kRecheck:
            SetStatusMessage("Rechecked the selected messages for junk.");
            break;
    }
}

void HaikuMainWindow::HandleFilterSelectedMessages() {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    if (!shell_host_.ApplyFiltersToMessages(current_mailbox_id_, selected_ids)) {
        SetStatusMessage("Unable to apply filters to the selected messages.");
        return;
    }
    shell_host_.OpenToolWindow("filter-report");
    SetStatusMessage("Applied filters to the selected messages.");
}

void HaikuMainWindow::HandleMakeFilter() {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    std::string error_message;
    const auto rule = shell_host_.CreateManualFilterFromMessages(current_mailbox_id_, selected_ids, &error_message);
    if (!rule) {
        SetStatusMessage(error_message.empty() ? "Unable to make a filter from the selected messages."
                                               : error_message);
        return;
    }
    shell_host_.OpenToolWindow("filters");
    SetStatusMessage("Created a filter from the selected messages.");
}

void HaikuMainWindow::HandleSetSelectedPriority(int priority_value) {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    std::string error_message;
    for (const auto& message_id : selected_ids) {
        auto record = shell_host_.Messages().GetMessage(current_mailbox_id_, message_id);
        if (!record) {
            continue;
        }
        record->compose_options.priority = static_cast<ComposePriority>(priority_value);
        record->updated_at = std::time(nullptr);
        if (!shell_host_.Messages().SaveMessage(*record, &error_message)) {
            SetStatusMessage(error_message.empty() ? "Unable to update message priority."
                                                   : error_message);
            return;
        }
    }
    shell_host_.ReloadWorkspace();
    SetStatusMessage("Set message priority to " +
                     PriorityMenuLabel(static_cast<ComposePriority>(priority_value)) + ".");
}

void HaikuMainWindow::HandleMakeNickname() {
    const auto selected_message_id = FirstSelectedMessageId();
    if (!selected_message_id) {
        return;
    }
    const auto detail = shell_host_.WorkspaceMessageDetail(*selected_message_id);
    if (!detail) {
        return;
    }
    NicknameEntry entry;
    entry.nickname = ExtractDisplayName(detail->sender);
    entry.full_name = ExtractDisplayName(detail->sender);
    entry.addresses = {ExtractEmailAddress(detail->sender)};
    entry.recipient_list = true;
    if (entry.nickname.empty()) {
        entry.nickname = entry.addresses.front();
    }
    shell_host_.Nicknames().AddOrReplace(entry);
    std::string error_message;
    shell_host_.Nicknames().SaveToFile(shell_host_.DataRootPath() / "Nicknames.txt", &error_message);
    shell_host_.ReloadWorkspace();
    shell_host_.OpenToolWindow("nicknames");
    SetStatusMessage("Created a nickname from the selected message.");
}

void HaikuMainWindow::HandleDynamicRecipientCompose(uint32 command, std::string_view nickname_name) {
    const auto entry = shell_host_.Nicknames().FindNickname(nickname_name);
    if (!entry) {
        SetStatusMessage("Unable to resolve the selected nickname.");
        return;
    }

    ComposeMessage compose;
    switch (command) {
        case kDynamicNewRecipientMessage:
            compose = BuildDefaultComposeMessage(shell_host_);
            break;
        case kDynamicForwardRecipientMessage:
            if (const auto selected = FirstSelectedMessageId()) {
                if (const auto response =
                        shell_host_.BuildResponseMessage(HaikuShellHost::MessageResponseKind::kForward,
                                                         current_mailbox_id_,
                                                         *selected)) {
                    compose = *response;
                    break;
                }
            }
            SetStatusMessage("Unable to prepare a forwarded message for the selected recipient.");
            return;
        case kDynamicRedirectRecipientMessage:
            if (const auto selected = FirstSelectedMessageId()) {
                if (const auto response =
                        shell_host_.BuildResponseMessage(HaikuShellHost::MessageResponseKind::kRedirect,
                                                         current_mailbox_id_,
                                                         *selected)) {
                    compose = *response;
                    break;
                }
            }
            SetStatusMessage("Unable to prepare a redirected message for the selected recipient.");
            return;
        default:
            return;
    }

    compose.headers.to = JoinCommaList(entry->addresses);
    if (shell_host_.OpenComposer(compose)) {
        SetStatusMessage("Opened a compose window for the selected recipient.");
    } else {
        SetStatusMessage("Unable to open a compose window for the selected recipient.");
    }
}

void HaikuMainWindow::HandleSelectedTextUrlCommand(int slot) {
    if (slot <= 0) {
        return;
    }
    const std::string selected_text = SelectedFocusedText();
    if (selected_text.empty()) {
        SetStatusMessage("Select text before using this command.");
        return;
    }
    const auto action = std::find_if(selected_text_url_actions_.begin(),
                                     selected_text_url_actions_.end(),
                                     [slot](const auto& entry) { return entry.slot == slot; });
    if (action == selected_text_url_actions_.end()) {
        SetStatusMessage("This selected-text URL command is not configured.");
        return;
    }
    const std::string target = BuildSelectedTextUrl(*action, selected_text);
    if (LaunchExternalTarget(target)) {
        SetStatusMessage("Opened the selected-text URL target.");
    } else {
        SetStatusMessage("Unable to open the selected-text URL target.");
    }
}

void HaikuMainWindow::HandleWindowArrangement(HaikuShellHost::WindowArrangeMode mode) {
    if (shell_host_.ArrangeManagedWindows(mode)) {
        SetStatusMessage("Updated the Hemera window arrangement.");
    } else {
        SetStatusMessage("Unable to update the Hemera window arrangement.");
    }
}

void HaikuMainWindow::HandleCloseAllWindows() {
    if (shell_host_.CloseAllManagedWindows()) {
        SetStatusMessage("Closed the secondary Hemera windows.");
    } else {
        SetStatusMessage("No secondary Hemera windows were open.");
    }
}

void HaikuMainWindow::HandlePrintSelectedMessage(bool preview) {
    const auto selected_message_id = FirstSelectedMessageId();
    if (!selected_message_id) {
        return;
    }
    const auto detail = shell_host_.WorkspaceMessageDetail(*selected_message_id);
    if (!detail) {
        return;
    }
    const auto request = BuildRenderRequest(*detail);
    if (request && preview_web_view_->Load(*request) && preview_web_view_->CanPrint()) {
        const bool rendered = preview ? preview_web_view_->PrintPreview()
                                      : (preview_web_view_->CanDirectPrint() && preview_web_view_->DirectPrint());
        if (rendered) {
            SetStatusMessage(preview ? "Opened print preview for the selected message."
                                     : "Sent the selected message directly to the printer.");
            return;
        }
    }
    SetStatusMessage("Printing is not currently available for the selected message.");
}

void HaikuMainWindow::HandleFindMessages() {
    if (current_mailbox_id_.empty()) {
        return;
    }
    shell_host_.QueuePendingSearch({"",
                                    HaikuShellHost::SearchRequest::Scope::kCurrentMailbox,
                                    current_mailbox_id_});
    shell_host_.OpenToolWindow("search");
    SetStatusMessage("Opened mailbox-scoped search.");
}

void HaikuMainWindow::HandleRedownloadSelectedMessages(bool full) {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    const bool success = shell_host_.RedownloadImapMessages(current_mailbox_id_, selected_ids, full);
    PopulateTaskStatus();
    SetStatusMessage(success ? (full ? "Full message redownload queued." : "Default message redownload queued.")
                             : (full ? "Unable to queue full message redownload."
                                     : "Unable to queue default message redownload."));
}

void HaikuMainWindow::HandleClearCachedSelectedMessages() {
    const auto selected_ids = SelectedMessageIds();
    if (selected_ids.empty()) {
        return;
    }
    if (!shell_host_.ClearCachedImapMessages(current_mailbox_id_, selected_ids)) {
        SetStatusMessage("Unable to clear cached IMAP message data.");
        return;
    }
    SetStatusMessage("Cleared cached IMAP message data.");
}

void HaikuMainWindow::RefreshWorkspace() {
    gui_preferences_ = GuiPreferencesFromSettings(shell_host_.Settings());
    mailbox_ui_ = shell_host_.MailboxUi();
    search_bar_mode_ = gui_preferences_.search_bar_mode;
    ApplyFindShortcutMapping();
    if (!label_menu_items_.empty()) {
        label_menu_items_.front()->SetLabel("No Label");
        for (std::size_t index = 1; index < label_menu_items_.size(); ++index) {
            label_menu_items_[index]->SetLabel(MailboxLabelName(mailbox_ui_, static_cast<int>(index)).c_str());
        }
    }
    PopulateWorkspace();
    PopulateTaskStatus();
    RefreshDynamicMessageMenus();
    RefreshRecentMailboxMenu();
    RefreshSelectedTextUrlActions();
    RebuildToolbar();
    RefreshSearchBarRecentMenu();
    UpdateSearchBarState();
    ApplyGuiPreferences();
    UpdateCommandState();
}

}  // namespace hemera::haiku
