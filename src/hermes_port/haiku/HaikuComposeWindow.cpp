#include "HaikuComposeWindow.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <sys/utsname.h>

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <Clipboard.h>
#include <Entry.h>
#include <FilePanel.h>
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
#include <MessageRunner.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <Path.h>
#include <ScrollView.h>
#include <Size.h>
#include <SplitView.h>
#include <StringItem.h>
#include <StringView.h>
#include <Tab.h>
#include <TabView.h>
#include <TextControl.h>

#include "HaikuFindSupport.h"
#include "HaikuShellHost.h"
#include "HaikuToolbarSupport.h"
#include "HaikuWebKitSupport.h"
#include "PaigeEditorView.h"
#include "hermes/ComposeController.h"
#include "hermes/ComposeQueue.h"
#include "hermes/HemeraIdentity.h"
#include "hermes/HunspellSpellService.h"
#include "hermes/MoodWatchAnalyzer.h"
#include "hermes/NicknameStore.h"
#include "hermes/PaigeRichTextSurface.h"
#include "hermes/RichTextFormat.h"
#include "hermes/RichTextSurface.h"
#include "hermes/ToolbarConfiguration.h"

namespace hemera::haiku {

namespace {

constexpr uint32_t kSaveDraftMessage = 'sdrf';
constexpr uint32_t kQueueMessage = 'queu';
constexpr uint32_t kUndoMessage = 'undo';
constexpr uint32_t kRedoMessage = 'redo';
constexpr uint32_t kCutMessage = 'xcut';
constexpr uint32_t kCopyMessage = 'copy';
constexpr uint32_t kPasteMessage = 'past';
constexpr uint32_t kPasteSpecialMessage = 'pspc';
constexpr uint32_t kPasteAsQuotationMessage = 'psqt';
constexpr uint32_t kSelectAllMessage = 'sall';
constexpr uint32_t kCheckDocumentMessage = 'spck';
constexpr uint32_t kIgnoreWordMessage = 'spig';
constexpr uint32_t kAddWordMessage = 'spad';
constexpr uint32_t kReplaceCurrentMessage = 'sprp';
constexpr uint32_t kStationeryMessage = 'stny';
constexpr uint32_t kSignatureMessage = 'sgnt';
constexpr uint32_t kPersonaMessage = 'pers';
constexpr uint32_t kSurfaceMessage = 'srfc';
constexpr uint32_t kPriorityMessage = 'prio';
constexpr uint32_t kEncodingMessage = 'enco';
constexpr uint32_t kComposeOptionChangedMessage = 'copt';
constexpr uint32_t kIdleTickMessage = 'idlt';
constexpr uint32_t kDiagnosticMessage = 'diag';
constexpr uint32_t kHeaderModifiedMessage = 'hedr';
constexpr uint32_t kAddAttachmentMessage = 'atag';
constexpr uint32_t kRemoveAttachmentMessage = 'atrm';
constexpr uint32_t kAddAttachmentSelected = 'atrs';
constexpr uint32_t kInsertPictureSelected = 'iprs';
constexpr uint32_t kCustomizeToolbarMessage = 'cttb';
constexpr uint32_t kSendImmediatelyComposeMessage = 'simm';
constexpr uint32_t kSaveAsStationeryMessage = 'svst';
constexpr uint32_t kSaveAsStationeryConfirmedMessage = 'sscf';
constexpr uint32_t kPrintComposeMessage = 'cprt';
constexpr uint32_t kPrintOneComposeMessage = 'cp1o';
constexpr uint32_t kPrintPreviewComposeMessage = 'cppv';
constexpr uint32_t kToggleHeadersMessage = 'tghd';
constexpr uint32_t kInsertSystemConfigurationMessage = 'insc';
constexpr uint32_t kInsertRecipientsMessage = 'inrc';
constexpr uint32_t kInsertPictureMessage = 'insp';
constexpr uint32_t kDynamicInsertRecipientMessage = 'dirc';
constexpr uint32_t kFormatPlainMessage = 'fcpl';
constexpr uint32_t kFormatBoldMessage = 'fcbd';
constexpr uint32_t kFormatItalicMessage = 'fcit';
constexpr uint32_t kFormatUnderlineMessage = 'fcun';
constexpr uint32_t kFormatStrikeoutMessage = 'fcst';
constexpr uint32_t kFormatFixedWidthMessage = 'fcfw';
constexpr uint32_t kFormatAddQuoteMessage = 'fcaq';
constexpr uint32_t kFormatRemoveQuoteMessage = 'fcrq';
constexpr uint32_t kFormatIndentInMessage = 'fcin';
constexpr uint32_t kFormatIndentOutMessage = 'fcout';
constexpr uint32_t kFormatNormalMarginsMessage = 'fcnm';
constexpr uint32_t kFormatAlignLeftMessage = 'fcal';
constexpr uint32_t kFormatAlignCenterMessage = 'fcac';
constexpr uint32_t kFormatAlignRightMessage = 'fcar';
constexpr uint32_t kFormatBulletedListMessage = 'fcbl';
constexpr uint32_t kFormatInsertLinkMessage = 'fcil';
constexpr uint32_t kFormatClearMessage = 'fccl';
constexpr uint32_t kFormatTextColorMessage = 'fctc';
constexpr uint32_t kFormatTextSizeMessage = 'fcts';
constexpr uint32_t kFormatPainterMessage = 'fcfp';
constexpr uint32_t kInsertDownloadablePictureMessage = 'idlp';
constexpr uint32_t kInsertHorizontalRuleMessage = 'ihrl';
constexpr uint32_t kWrapSelectionMessage = 'wrap';
constexpr uint32_t kInsertLinkConfirmedMessage = 'ilcf';
constexpr uint32_t kTextColorConfirmedMessage = 'tccf';
constexpr uint32_t kTextSizeConfirmedMessage = 'tscf';
constexpr uint32_t kDownloadablePictureConfirmedMessage = 'dpcf';
constexpr uint32_t kFindMessage = 'find';
constexpr uint32_t kFindAgainMessage = 'fdag';
constexpr uint32_t kFindConfirmedMessage = 'fdcf';
constexpr uint32_t kFindClosedMessage = 'fdcl';
constexpr uint32_t kCancelFormatPainterMessage = 'cfpt';

constexpr float kHeaderDivider = 76.0f;

std::filesystem::path SourceRoot() {
#ifdef HERMES_SOURCE_ROOT
    return std::filesystem::path(HERMES_SOURCE_ROOT);
#else
    return std::filesystem::current_path();
#endif
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

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::optional<std::string> ClipboardText() {
    if (be_clipboard == nullptr || !be_clipboard->Lock()) {
        return std::nullopt;
    }
    std::optional<std::string> text;
    if (BMessage* data = be_clipboard->Data()) {
        const char* clipboard_text = nullptr;
        if (data->FindString("text/plain", &clipboard_text) == B_OK && clipboard_text != nullptr) {
            text = clipboard_text;
        }
    }
    be_clipboard->Unlock();
    return text;
}

std::string QuoteText(std::string_view text) {
    std::string quoted;
    std::stringstream stream{std::string(text)};
    std::string line;
    bool first = true;
    while (std::getline(stream, line)) {
        if (!first) {
            quoted.push_back('\n');
        }
        first = false;
        quoted += "> " + line;
    }
    if (quoted.empty()) {
        quoted = "> ";
    }
    return quoted;
}

std::string MimeTypeForImagePath(const std::filesystem::path& path) {
    const std::string ext = ToLowerCopy(path.extension().string());
    if (ext == ".png") {
        return "image/png";
    }
    if (ext == ".jpg" || ext == ".jpeg") {
        return "image/jpeg";
    }
    if (ext == ".gif") {
        return "image/gif";
    }
    if (ext == ".webp") {
        return "image/webp";
    }
    if (ext == ".bmp") {
        return "image/bmp";
    }
    if (ext == ".svg") {
        return "image/svg+xml";
    }
    return {};
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

std::string Lowercase(std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}

std::optional<TextSelection> FindNextCaseInsensitive(std::string_view haystack,
                                                     std::string_view needle,
                                                     std::size_t start_offset) {
    if (needle.empty() || haystack.empty()) {
        return std::nullopt;
    }
    const std::string lowered_haystack = Lowercase(haystack);
    const std::string lowered_needle = Lowercase(needle);
    std::size_t position = lowered_haystack.find(lowered_needle, start_offset);
    if (position == std::string::npos && start_offset > 0) {
        position = lowered_haystack.find(lowered_needle);
    }
    if (position == std::string::npos) {
        return std::nullopt;
    }
    return TextSelection{position, needle.size()};
}

std::string DefaultStationeryNameForMessage(const ComposeMessage& message) {
    if (!message.stationery_name.empty()) {
        return message.stationery_name;
    }
    if (!TrimWhitespace(message.headers.subject).empty()) {
        return TrimWhitespace(message.headers.subject);
    }
    return "New Stationery";
}

bool LaunchPath(const std::filesystem::path& path) {
    if (be_roster == nullptr) {
        return false;
    }
    entry_ref ref;
    BEntry entry(path.c_str(), true);
    if (entry.InitCheck() != B_OK || entry.GetRef(&ref) != B_OK) {
        return false;
    }
    return be_roster->Launch(&ref) == B_OK;
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

bool LooksLikeHtmlDocument(std::string_view html) {
    std::string lowered(html);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered.find("<html") != std::string::npos || lowered.find("<!doctype") != std::string::npos;
}

std::string WrapPrintHtmlDocument(std::string_view title, std::string_view body_html) {
    return "<!doctype html><html><head><meta charset=\"utf-8\"><title>" + EscapeHtmlText(title) +
           "</title><style>body{font-family:sans-serif;margin:24px;line-height:1.5;}"
           "header{margin-bottom:20px;padding-bottom:12px;border-bottom:1px solid #ccc;}"
           "dl{display:grid;grid-template-columns:max-content 1fr;gap:6px 12px;margin:0;}"
           "dt{font-weight:600;}dd{margin:0;}img{max-width:100%;}"
           "pre{white-space:pre-wrap;word-break:break-word;}</style></head><body>" +
           std::string(body_html) + "</body></html>";
}

std::string BuildComposePrintHeaderHtml(const ComposeMessage& snapshot) {
    std::ostringstream html;
    html << "<header><dl>";
    const auto add_field = [&html](std::string_view label, const std::string& value) {
        if (value.empty()) {
            return;
        }
        html << "<dt>" << EscapeHtmlText(label) << "</dt><dd>" << EscapeHtmlText(value) << "</dd>";
    };
    add_field("To", snapshot.headers.to);
    add_field("Cc", snapshot.headers.cc);
    add_field("Bcc", snapshot.headers.bcc);
    add_field("Reply-To", snapshot.headers.reply_to);
    add_field("Subject", snapshot.headers.subject);
    add_field("Persona", snapshot.headers.from_persona);
    if (!snapshot.attachments.empty()) {
        std::ostringstream attachments;
        for (std::size_t index = 0; index < snapshot.attachments.size(); ++index) {
            if (index != 0) {
                attachments << ", ";
            }
            attachments << snapshot.attachments[index].display_name;
        }
        add_field("Attachments", attachments.str());
    }
    html << "</dl></header>";
    return html.str();
}

std::string BuildComposePrintHeaderText(const ComposeMessage& snapshot) {
    std::ostringstream printable;
    const auto add_field = [&printable](std::string_view label, const std::string& value) {
        if (!value.empty()) {
            printable << label << ": " << value << '\n';
        }
    };
    add_field("To", snapshot.headers.to);
    add_field("Cc", snapshot.headers.cc);
    add_field("Bcc", snapshot.headers.bcc);
    add_field("Reply-To", snapshot.headers.reply_to);
    add_field("Subject", snapshot.headers.subject);
    add_field("Persona", snapshot.headers.from_persona);
    if (!snapshot.attachments.empty()) {
        std::ostringstream attachments;
        for (std::size_t index = 0; index < snapshot.attachments.size(); ++index) {
            if (index != 0) {
                attachments << ", ";
            }
            attachments << snapshot.attachments[index].display_name;
        }
        add_field("Attachments", attachments.str());
    }
    return printable.str();
}

std::string BuildSystemConfigurationBlock(const ComposeMessage& message,
                                          const std::filesystem::path& data_root) {
    struct utsname system_name {};
    const bool have_uname = ::uname(&system_name) == 0;

    std::ostringstream block;
    block << "----- System Configuration -----\n";
    block << "Application: " << hermes::kHemeraLongProductName << " (Haiku shell)\n";
    if (have_uname) {
        block << "System: " << system_name.sysname << ' ' << system_name.release << '\n';
        block << "Version: " << system_name.version << '\n';
        block << "Machine: " << system_name.machine << '\n';
    }
    if (!message.headers.from_persona.empty()) {
        block << "Persona: " << message.headers.from_persona << '\n';
    }
    if (!message.signature_name.empty()) {
        block << "Signature: " << message.signature_name << '\n';
    }
    if (!message.stationery_name.empty()) {
        block << "Stationery: " << message.stationery_name << '\n';
    }
    block << "Data Root: " << data_root.string() << '\n';
    block << "--------------------------------";
    return block.str();
}

std::string ControlValue(BTextControl* control) {
    return control != nullptr && control->Text() != nullptr ? control->Text() : "";
}

std::string DiagnosticSummary(const hermes::ComposeVisualDiagnostic& diagnostic) {
    return diagnostic.label + ": " + diagnostic.message;
}

std::string AttachmentSummary(const hermes::ComposeAttachment& attachment) {
    std::ostringstream stream;
    stream << (attachment.display_name.empty() ? "(unnamed attachment)" : attachment.display_name);
    if (attachment.size > 0) {
        stream << " (" << attachment.size << " bytes)";
    }
    return stream.str();
}

void SetDivider(BTextControl* control) {
    if (control != nullptr) {
        control->SetDivider(kHeaderDivider);
    }
}

const char* PriorityLabel(hermes::ComposePriority priority) {
    switch (priority) {
        case hermes::ComposePriority::kHighest:
            return "Highest";
        case hermes::ComposePriority::kHigh:
            return "High";
        case hermes::ComposePriority::kNormal:
            return "Normal";
        case hermes::ComposePriority::kLow:
            return "Low";
        case hermes::ComposePriority::kLowest:
            return "Lowest";
    }
    return "Normal";
}

const char* EncodingLabel(hermes::AttachmentEncodingMode encoding) {
    switch (encoding) {
        case hermes::AttachmentEncodingMode::kMime:
            return "MIME";
        case hermes::AttachmentEncodingMode::kBinHex:
            return "BinHex";
        case hermes::AttachmentEncodingMode::kUuencode:
            return "Uuencode";
    }
    return "MIME";
}

const char* SurfaceLabel(bool html_surface) {
    return html_surface ? "HTML" : "Paige";
}

std::optional<ComposeEditorCommand> EditorCommandForMessage(uint32_t what) {
    switch (what) {
        case kFormatPlainMessage:
            return ComposeEditorCommand::kPlain;
        case kFormatBoldMessage:
            return ComposeEditorCommand::kBold;
        case kFormatItalicMessage:
            return ComposeEditorCommand::kItalic;
        case kFormatUnderlineMessage:
            return ComposeEditorCommand::kUnderline;
        case kFormatStrikeoutMessage:
            return ComposeEditorCommand::kStrikeout;
        case kFormatFixedWidthMessage:
            return ComposeEditorCommand::kFixedWidth;
        case kFormatAddQuoteMessage:
            return ComposeEditorCommand::kAddQuote;
        case kFormatRemoveQuoteMessage:
            return ComposeEditorCommand::kRemoveQuote;
        case kFormatIndentInMessage:
            return ComposeEditorCommand::kIndentIn;
        case kFormatIndentOutMessage:
            return ComposeEditorCommand::kIndentOut;
        case kFormatNormalMarginsMessage:
            return ComposeEditorCommand::kNormalMargins;
        case kFormatAlignLeftMessage:
            return ComposeEditorCommand::kAlignLeft;
        case kFormatAlignCenterMessage:
            return ComposeEditorCommand::kAlignCenter;
        case kFormatAlignRightMessage:
            return ComposeEditorCommand::kAlignRight;
        case kFormatBulletedListMessage:
            return ComposeEditorCommand::kBulletedList;
        case kFormatInsertLinkMessage:
            return ComposeEditorCommand::kInsertLink;
        case kFormatClearMessage:
            return ComposeEditorCommand::kClearFormatting;
        case kFormatTextColorMessage:
            return ComposeEditorCommand::kTextColor;
        case kFormatTextSizeMessage:
            return ComposeEditorCommand::kTextSize;
        case kFormatPainterMessage:
            return ComposeEditorCommand::kFormatPainter;
        case kInsertDownloadablePictureMessage:
            return ComposeEditorCommand::kInsertDownloadablePicture;
        case kInsertHorizontalRuleMessage:
            return ComposeEditorCommand::kInsertHorizontalRule;
        case kWrapSelectionMessage:
            return ComposeEditorCommand::kWrapSelection;
        default:
            return std::nullopt;
    }
}

bool PreferHtmlSurfaceForDocument(const hermes::RichTextDocument& document) {
    return document.styled_source == hermes::StyledDocumentSource::kHtml || hermes::RequiresHtmlSurface(document);
}

void SetCheckbox(BCheckBox* checkbox, bool enabled) {
    if (checkbox != nullptr) {
        checkbox->SetValue(enabled ? B_CONTROL_ON : B_CONTROL_OFF);
    }
}

class TextPromptWindow final : public BWindow {
public:
    TextPromptWindow(const char* title,
                     const char* label,
                     const std::string& initial_value,
                     const char* response_key,
                     const BMessenger& target,
                     BMessage payload)
        : BWindow(BRect(0, 0, 420, 140),
                  title,
                  B_TITLED_WINDOW,
                  B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
          response_key_(response_key == nullptr ? "value" : response_key),
          target_(target),
          payload_(std::move(payload)) {
        input_ = new BTextControl("prompt-input", label, initial_value.c_str(), nullptr);
        auto* cancel = new BButton("cancel-button", "Cancel", new BMessage(B_QUIT_REQUESTED));
        auto* ok = new BButton("ok-button", "OK", new BMessage(kFindConfirmedMessage));
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
        if (message->what == kFindConfirmedMessage) {
            BMessage response(payload_);
            response.AddString(response_key_.c_str(), input_->Text());
            target_.SendMessage(&response);
            PostMessage(B_QUIT_REQUESTED);
            return;
        }
        BWindow::MessageReceived(message);
    }

private:
    std::string response_key_;
    BMessenger target_;
    BMessage payload_;
    BTextControl* input_ = nullptr;
};

}  // namespace

HaikuComposeWindow::HaikuComposeWindow(HaikuShellHost& shell_host, const ComposeMessage& message)
    : BWindow(BRect(140, 140, 1180, 920),
              "Compose Message",
              B_TITLED_WINDOW,
              B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
      shell_host_(shell_host),
      gui_preferences_(GuiPreferencesFromSettings(shell_host.Settings())),
      compose_cache_key_(message.id.empty() ? "compose-window" : message.id),
      spell_service_(std::make_unique<hermes::HunspellSpellService>(
          SourceRoot() / "third_party" / "hunspell" / "tests" / "base_utf.aff",
          SourceRoot() / "third_party" / "hunspell" / "tests" / "base_utf.dic")),
      mood_watch_analyzer_(std::make_unique<hermes::TaeMoodWatchAnalyzer>(
          SourceRoot() / "src" / "legacy_transplants" / "tae" / "FlameLex.dat")) {
    std::string ignored;
    (void)shell_host_.Runtime().Initialize(&ignored);

    menu_bar_ = new BMenuBar("compose-menu-bar");
    auto* file_menu = new BMenu("File");
    file_menu->AddItem(new BMenuItem("Save Draft", new BMessage(kSaveDraftMessage)));
    file_menu->AddItem(new BMenuItem("Queue", new BMessage(kQueueMessage)));
    file_menu->AddItem(new BMenuItem("Send Immediately", new BMessage(kSendImmediatelyComposeMessage), 'R'));
    print_item_ = new BMenuItem("Print", new BMessage(kPrintComposeMessage), 'P');
    print_one_item_ = new BMenuItem("Print One", new BMessage(kPrintOneComposeMessage));
    print_preview_item_ =
        new BMenuItem("Print Preview", new BMessage(kPrintPreviewComposeMessage));
    file_menu->AddItem(print_item_);
    file_menu->AddItem(print_one_item_);
    file_menu->AddItem(print_preview_item_);
    file_menu->AddItem(new BMenuItem("Save as Stationery" B_UTF8_ELLIPSIS,
                                     new BMessage(kSaveAsStationeryMessage)));
    file_menu->AddSeparatorItem();
    file_menu->AddItem(new BMenuItem("Close", new BMessage(B_QUIT_REQUESTED)));
    menu_bar_->AddItem(file_menu);

    auto* edit_menu = new BMenu("Edit");
    edit_menu->AddItem(new BMenuItem("Undo", new BMessage(kUndoMessage)));
    edit_menu->AddItem(new BMenuItem("Redo", new BMessage(kRedoMessage)));
    edit_menu->AddSeparatorItem();
    edit_menu->AddItem(new BMenuItem("Cut", new BMessage(kCutMessage)));
    edit_menu->AddItem(new BMenuItem("Copy", new BMessage(kCopyMessage)));
    edit_menu->AddItem(new BMenuItem("Paste", new BMessage(kPasteMessage)));
    edit_menu->AddItem(new BMenuItem("Paste Special", new BMessage(kPasteSpecialMessage)));
    edit_menu->AddItem(new BMenuItem("Paste as Quotation", new BMessage(kPasteAsQuotationMessage)));
    edit_menu->AddItem(new BMenuItem("Select All", new BMessage(kSelectAllMessage)));
    edit_menu->AddSeparatorItem();
    insert_recipients_menu_ = new BMenu("Insert Recipients");
    edit_menu->AddItem(insert_recipients_menu_);
    edit_menu->AddSeparatorItem();
    edit_menu->AddItem(new BMenuItem("Find" B_UTF8_ELLIPSIS, new BMessage(kFindMessage), 'F'));
    find_again_item_ = new BMenuItem("Find Again", new BMessage(kFindAgainMessage));
    edit_menu->AddItem(find_again_item_);
    menu_bar_->AddItem(edit_menu);

    auto* format_menu = new BMenu("Format");
    const auto add_editor_command = [this, format_menu](const char* label,
                                                        uint32_t message_what,
                                                        ComposeEditorCommand command) {
        auto* item = new BMenuItem(label, new BMessage(message_what));
        format_menu->AddItem(item);
        editor_command_bindings_.push_back({command, item});
    };
    add_editor_command("Plain", kFormatPlainMessage, ComposeEditorCommand::kPlain);
    add_editor_command("Bold", kFormatBoldMessage, ComposeEditorCommand::kBold);
    add_editor_command("Italic", kFormatItalicMessage, ComposeEditorCommand::kItalic);
    add_editor_command("Underline", kFormatUnderlineMessage, ComposeEditorCommand::kUnderline);
    add_editor_command("Strikeout", kFormatStrikeoutMessage, ComposeEditorCommand::kStrikeout);
    add_editor_command("Fixed Width", kFormatFixedWidthMessage, ComposeEditorCommand::kFixedWidth);
    format_menu->AddSeparatorItem();
    add_editor_command("Add Quote", kFormatAddQuoteMessage, ComposeEditorCommand::kAddQuote);
    add_editor_command("Remove Quote", kFormatRemoveQuoteMessage, ComposeEditorCommand::kRemoveQuote);
    add_editor_command("Indent In", kFormatIndentInMessage, ComposeEditorCommand::kIndentIn);
    add_editor_command("Indent Out", kFormatIndentOutMessage, ComposeEditorCommand::kIndentOut);
    add_editor_command("Normal Margins", kFormatNormalMarginsMessage, ComposeEditorCommand::kNormalMargins);
    format_menu->AddSeparatorItem();
    add_editor_command("Align Left", kFormatAlignLeftMessage, ComposeEditorCommand::kAlignLeft);
    add_editor_command("Center", kFormatAlignCenterMessage, ComposeEditorCommand::kAlignCenter);
    add_editor_command("Align Right", kFormatAlignRightMessage, ComposeEditorCommand::kAlignRight);
    add_editor_command("Bulleted List", kFormatBulletedListMessage, ComposeEditorCommand::kBulletedList);
    format_menu->AddSeparatorItem();
    add_editor_command("Insert Link" B_UTF8_ELLIPSIS,
                       kFormatInsertLinkMessage,
                       ComposeEditorCommand::kInsertLink);
    add_editor_command("Text Color" B_UTF8_ELLIPSIS,
                       kFormatTextColorMessage,
                       ComposeEditorCommand::kTextColor);
    add_editor_command("Text Size" B_UTF8_ELLIPSIS,
                       kFormatTextSizeMessage,
                       ComposeEditorCommand::kTextSize);
    add_editor_command("Clear Formatting", kFormatClearMessage, ComposeEditorCommand::kClearFormatting);
    add_editor_command("Format Painter", kFormatPainterMessage, ComposeEditorCommand::kFormatPainter);
    menu_bar_->AddItem(format_menu);

    auto* spelling_menu = new BMenu("Spelling");
    spelling_menu->AddItem(new BMenuItem("Check Document", new BMessage(kCheckDocumentMessage)));
    spelling_menu->AddItem(new BMenuItem("Ignore Word", new BMessage(kIgnoreWordMessage)));
    spelling_menu->AddItem(new BMenuItem("Add Word", new BMessage(kAddWordMessage)));
    spelling_menu->AddItem(new BMenuItem("Replace Current", new BMessage(kReplaceCurrentMessage)));
    menu_bar_->AddItem(spelling_menu);

    auto* attachments_menu = new BMenu("Attachments");
    attachments_menu->AddItem(new BMenuItem("Add Attachment", new BMessage(kAddAttachmentMessage)));
    attachments_menu->AddItem(new BMenuItem("Remove Attachment", new BMessage(kRemoveAttachmentMessage)));
    menu_bar_->AddItem(attachments_menu);

    auto* insert_menu = new BMenu("Insert");
    insert_menu->AddItem(
        new BMenuItem("System Configuration", new BMessage(kInsertSystemConfigurationMessage)));
    insert_picture_item_ = new BMenuItem("Insert Picture" B_UTF8_ELLIPSIS, new BMessage(kInsertPictureMessage));
    insert_menu->AddItem(insert_picture_item_);
    const auto add_insert_command = [this, insert_menu](const char* label,
                                                        uint32_t message_what,
                                                        ComposeEditorCommand command) {
        auto* item = new BMenuItem(label, new BMessage(message_what));
        insert_menu->AddItem(item);
        editor_command_bindings_.push_back({command, item});
    };
    add_insert_command("Insert Downloadable Picture" B_UTF8_ELLIPSIS,
                       kInsertDownloadablePictureMessage,
                       ComposeEditorCommand::kInsertDownloadablePicture);
    add_insert_command("Insert Horizontal Line",
                       kInsertHorizontalRuleMessage,
                       ComposeEditorCommand::kInsertHorizontalRule);
    add_insert_command("Wrap Selection", kWrapSelectionMessage, ComposeEditorCommand::kWrapSelection);
    menu_bar_->AddItem(insert_menu);

    auto* view_menu = new BMenu("View");
    show_headers_item_ =
        new BMenuItem("Show Extended Headers", new BMessage(kToggleHeadersMessage));
    show_headers_item_->SetMarked(true);
    view_menu->AddItem(show_headers_item_);
    view_menu->AddItem(new BMenuItem("Customize Toolbar" B_UTF8_ELLIPSIS,
                                     new BMessage(kCustomizeToolbarMessage)));
    menu_bar_->AddItem(view_menu);

    to_control_ = new BTextControl("to-control", "To", "", nullptr);
    cc_control_ = new BTextControl("cc-control", "Cc", "", nullptr);
    bcc_control_ = new BTextControl("bcc-control", "Bcc", "", nullptr);
    subject_control_ = new BTextControl("subject-control", "Subject", "", nullptr);
    reply_to_control_ = new BTextControl("reply-to-control", "Reply-To", "", nullptr);

    for (BTextControl* control :
         {to_control_, cc_control_, bcc_control_, subject_control_, reply_to_control_}) {
        SetDivider(control);
        control->SetTarget(this);
        control->SetModificationMessage(new BMessage(kHeaderModifiedMessage));
    }

    auto* persona_menu = new BPopUpMenu("persona-menu");
    persona_menu->SetRadioMode(true);
    persona_menu->SetLabelFromMarked(true);
    persona_field_ = new BMenuField("persona-field", "Persona", persona_menu);

    auto* surface_menu = new BPopUpMenu("surface-menu");
    surface_menu->SetRadioMode(true);
    surface_menu->SetLabelFromMarked(true);
    surface_field_ = new BMenuField("surface-field", "Surface", surface_menu);

    auto* priority_menu = new BPopUpMenu("priority-menu");
    priority_menu->SetRadioMode(true);
    priority_menu->SetLabelFromMarked(true);
    priority_field_ = new BMenuField("priority-field", "Priority", priority_menu);

    auto* encoding_menu = new BPopUpMenu("encoding-menu");
    encoding_menu->SetRadioMode(true);
    encoding_menu->SetLabelFromMarked(true);
    encoding_field_ = new BMenuField("encoding-field", "Encoding", encoding_menu);

    auto* stationery_menu = new BPopUpMenu("stationery-menu");
    stationery_menu->SetRadioMode(true);
    stationery_menu->SetLabelFromMarked(true);
    stationery_field_ = new BMenuField("stationery-field", "Stationery", stationery_menu);

    auto* signature_menu = new BPopUpMenu("signature-menu");
    signature_menu->SetRadioMode(true);
    signature_menu->SetLabelFromMarked(true);
    signature_field_ = new BMenuField("signature-field", "Signature", signature_menu);

    quoted_printable_box_ =
        new BCheckBox("quoted-printable-box", "Quoted Printable", new BMessage(kComposeOptionChangedMessage));
    text_as_document_box_ =
        new BCheckBox("text-document-box", "Text as Document", new BMessage(kComposeOptionChangedMessage));
    word_wrap_box_ =
        new BCheckBox("word-wrap-box", "Word Wrap", new BMessage(kComposeOptionChangedMessage));
    tabs_in_body_box_ =
        new BCheckBox("tabs-in-body-box", "Tabs in Body", new BMessage(kComposeOptionChangedMessage));
    keep_copies_box_ =
        new BCheckBox("keep-copies-box", "Keep Copies", new BMessage(kComposeOptionChangedMessage));
    return_receipt_box_ =
        new BCheckBox("return-receipt-box", "Return Receipt", new BMessage(kComposeOptionChangedMessage));

    banner_view_ = new BStringView("compose-banner", "Compose ready.");
    mood_view_ = new BStringView("compose-mood", "MoodWatch: ready");
    diagnostics_list_ = new BListView("compose-diagnostics");
    diagnostics_list_->SetSelectionMessage(new BMessage(kDiagnosticMessage));
    attachment_list_ = new BListView("compose-attachments");
    RecreateEditorSurface(message, PreferHtmlSurfaceForDocument(message.body));
    auto* diagnostics_scroll =
        new BScrollView("compose-diagnostics-scroll", diagnostics_list_, 0, false, true);
    auto* attachment_scroll =
        new BScrollView("compose-attachments-scroll", attachment_list_, 0, false, true);

    auto* add_attachment_button =
        new BButton("compose-add-attachment", "Add Attachment", new BMessage(kAddAttachmentMessage));
    auto* remove_attachment_button =
        new BButton("compose-remove-attachment", "Remove Attachment", new BMessage(kRemoveAttachmentMessage));

    auto* attachments_tab = new BGroupView(B_VERTICAL);
    BLayoutBuilder::Group<>(attachments_tab, B_VERTICAL, 8)
        .SetInsets(B_USE_SMALL_SPACING)
        .AddGroup(B_HORIZONTAL, 8)
            .Add(add_attachment_button)
            .Add(remove_attachment_button)
            .AddGlue()
        .End()
        .Add(attachment_scroll);

    utility_tabs_ = new BTabView("compose-utility-tabs");
    utility_tabs_->AddTab(diagnostics_scroll);
    if (BTab* tab = utility_tabs_->TabAt(0)) {
        tab->SetLabel("Diagnostics");
    }
    utility_tabs_->AddTab(attachments_tab);
    if (BTab* tab = utility_tabs_->TabAt(1)) {
        tab->SetLabel("Attachments");
    }

    utility_container_ = new BGroupView(B_VERTICAL);
    BLayoutBuilder::Group<>(utility_container_, B_VERTICAL, 0)
        .Add(utility_tabs_);

    editor_root_container_ = new BGroupView(B_VERTICAL);
    if (editor_host_ != nullptr && editor_host_->RootView() != nullptr) {
        BLayoutBuilder::Group<>(editor_root_container_, B_VERTICAL, 0).Add(editor_host_->RootView());
    }

    auto* editor_split = new BSplitView(B_VERTICAL);
    editor_split->AddChild(editor_root_container_);
    editor_split->AddChild(utility_container_);

    auto* primary_headers = new BGroupView(B_VERTICAL);
    BLayoutBuilder::Group<>(primary_headers, B_VERTICAL, 6)
        .Add(to_control_)
        .Add(cc_control_)
        .Add(bcc_control_)
        .Add(subject_control_);

    auto* secondary_headers = new BGroupView(B_VERTICAL);
    secondary_headers_container_ = secondary_headers;
    BLayoutBuilder::Group<>(secondary_headers, B_VERTICAL, 6)
        .Add(persona_field_)
        .Add(surface_field_)
        .Add(priority_field_)
        .Add(encoding_field_)
        .Add(reply_to_control_)
        .Add(stationery_field_)
        .Add(signature_field_);

    auto* option_toggles = new BGroupView(B_VERTICAL);
    BLayoutBuilder::Group<>(option_toggles, B_VERTICAL, 6)
        .AddGroup(B_HORIZONTAL, 10)
            .Add(quoted_printable_box_)
            .Add(text_as_document_box_)
            .Add(word_wrap_box_)
        .End()
        .AddGroup(B_HORIZONTAL, 10)
            .Add(tabs_in_body_box_)
            .Add(keep_copies_box_)
            .Add(return_receipt_box_)
        .End();

    toolbar_view_ = new BToolBar(B_HORIZONTAL);
    RebuildToolbar();

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu_bar_)
        .AddGroup(B_VERTICAL, 8)
            .SetInsets(B_USE_WINDOW_SPACING)
            .Add(toolbar_view_)
            .AddGroup(B_HORIZONTAL, 12)
                .Add(primary_headers, 2.4f)
                .Add(secondary_headers, 1.5f)
            .End()
            .Add(option_toggles)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(banner_view_, 1.0f)
                .Add(mood_view_)
            .End()
            .Add(editor_split)
        .End();

    PopulateMenus();
    LoadMessage(message);
    ApplyUtilityPaneSizing();
    ResetIdleClock();
    idle_runner_ =
        std::make_unique<BMessageRunner>(BMessenger(this), new BMessage(kIdleTickMessage), 250000, -1);
    AddShortcut(B_F3_KEY, B_NO_COMMAND_KEY, new BMessage(kFindAgainMessage));
    AddShortcut(B_ESCAPE, B_NO_COMMAND_KEY, new BMessage(kCancelFormatPainterMessage));
}

HaikuComposeWindow::~HaikuComposeWindow() = default;

void HaikuComposeWindow::MessageReceived(BMessage* message) {
    switch (message->what) {
        case kSaveDraftMessage:
            HandleSaveDraft();
            return;

        case kQueueMessage:
            HandleQueue(false);
            return;

        case kSendImmediatelyComposeMessage:
            HandleSendImmediately();
            return;

        case kSaveAsStationeryMessage:
            HandleSaveAsStationery();
            return;

        case kPrintComposeMessage:
        case kPrintOneComposeMessage:
            HandlePrint(false);
            return;

        case kPrintPreviewComposeMessage:
            HandlePrint(true);
            return;

        case kUndoMessage:
            if (controller_->Undo()) {
                RefreshFromController(true);
            }
            return;

        case kRedoMessage:
            if (controller_->Redo()) {
                RefreshFromController(true);
            }
            return;

        case kCutMessage:
            if (editor_host_ != nullptr && editor_host_->CutSelection()) {
                RefreshFromController(true);
            }
            return;

        case kCopyMessage:
            if (editor_host_ != nullptr) {
                (void)editor_host_->CopySelection();
            }
            return;

        case kPasteMessage:
            if (HandleClipboardAttachmentPaste()) {
                return;
            }
            if (editor_host_ != nullptr && editor_host_->Paste()) {
                RefreshFromController(true);
            }
            return;

        case kPasteSpecialMessage:
            if (HandleClipboardAttachmentPaste()) {
                return;
            }
            if (editor_host_ != nullptr && editor_host_->Paste()) {
                RefreshFromController(true);
            }
            return;

        case kPasteAsQuotationMessage:
            (void)HandlePasteAsQuotation();
            return;

        case kSelectAllMessage:
            if (editor_host_ != nullptr && editor_host_->SelectAllText()) {
                RefreshFromController(false);
            }
            return;

        case kFindMessage:
            HandleFind(false);
            return;

        case kFindAgainMessage:
            HandleFind(true);
            return;

        case kCheckDocumentMessage:
            transient_status_message_.clear();
            controller_->CheckDocument();
            RefreshFromController(true);
            return;

        case kIgnoreWordMessage:
            if (controller_->IgnoreCurrentWord()) {
                transient_status_message_.clear();
                RefreshFromController(true);
            }
            return;

        case kAddWordMessage:
            if (controller_->AddCurrentWord()) {
                transient_status_message_.clear();
                RefreshFromController(true);
            }
            return;

        case kReplaceCurrentMessage: {
            const auto suggestions = controller_->SuggestionsForCurrentIssue();
            if (!suggestions.empty() && controller_->ReplaceCurrent(suggestions.front())) {
                transient_status_message_.clear();
                RefreshFromController(true);
            }
            return;
        }

        case kAddAttachmentMessage:
            HandleAddAttachment();
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
            const auto actions = ComposeToolbarActionSpecs(shell_host_);
            const auto allowed_entries = ToolbarAllowedEntries(actions);
            auto configuration = ParseToolbarConfiguration(gui_preferences_.compose_toolbar_layout,
                                                           allowed_entries,
                                                           ComposeToolbarDefaultEntries());
            toolbar_customization_window_ = std::make_unique<HaikuToolbarCustomizationWindow>(
                "Customize Compose Toolbar",
                actions,
                ComposeToolbarDefaultEntries(),
                std::move(configuration),
                gui_preferences_.show_toolbar_tips,
                gui_preferences_.show_toolbar_large_buttons,
                [this](const hermes::ToolbarConfiguration& updated,
                       bool show_tool_tips,
                       bool large_buttons) {
                    gui_preferences_.compose_toolbar_layout = SerializeToolbarConfiguration(updated);
                    gui_preferences_.show_toolbar_tips = show_tool_tips;
                    gui_preferences_.show_toolbar_large_buttons = large_buttons;
                    RebuildToolbar();
                    PersistGuiPreferences();
                });
            toolbar_customization_window_->Show();
            return;
        }

        case kToggleHeadersMessage:
            HandleToggleHeaders();
            return;

        case kInsertSystemConfigurationMessage:
            HandleInsertSystemConfiguration();
            return;

        case kDynamicInsertRecipientMessage: {
            const char* nickname_name = nullptr;
            if (message->FindString("nickname_name", &nickname_name) == B_OK && nickname_name != nullptr) {
                HandleInsertRecipients(nickname_name);
            }
            return;
        }

        case kFormatPlainMessage:
        case kFormatBoldMessage:
        case kFormatItalicMessage:
        case kFormatUnderlineMessage:
        case kFormatStrikeoutMessage:
        case kFormatFixedWidthMessage:
        case kFormatAddQuoteMessage:
        case kFormatRemoveQuoteMessage:
        case kFormatIndentInMessage:
        case kFormatIndentOutMessage:
        case kFormatNormalMarginsMessage:
        case kFormatAlignLeftMessage:
        case kFormatAlignCenterMessage:
        case kFormatAlignRightMessage:
        case kFormatBulletedListMessage:
        case kFormatInsertLinkMessage:
        case kFormatClearMessage:
        case kFormatTextColorMessage:
        case kFormatTextSizeMessage:
        case kFormatPainterMessage:
        case kInsertDownloadablePictureMessage:
        case kInsertHorizontalRuleMessage:
        case kWrapSelectionMessage:
            if (const auto command = EditorCommandForMessage(message->what)) {
                HandleEditorCommand(*command);
            }
            return;

        case kInsertLinkConfirmedMessage: {
            const char* value = nullptr;
            if (message->FindString("value", &value) == B_OK && value != nullptr && editor_host_ != nullptr &&
                editor_host_->InsertLink(TrimWhitespace(value))) {
                HandleBodyEdited();
                transient_status_message_ = "Inserted link.";
                RefreshBanner();
            }
            return;
        }

        case kTextColorConfirmedMessage: {
            const char* value = nullptr;
            if (message->FindString("value", &value) == B_OK && value != nullptr && editor_host_ != nullptr &&
                editor_host_->ApplyTextColor(TrimWhitespace(value))) {
                HandleBodyEdited();
                transient_status_message_ = "Applied text color.";
                RefreshBanner();
            }
            return;
        }

        case kTextSizeConfirmedMessage: {
            const char* value = nullptr;
            if (message->FindString("value", &value) == B_OK && value != nullptr && editor_host_ != nullptr &&
                editor_host_->ApplyTextSize(TrimWhitespace(value))) {
                HandleBodyEdited();
                transient_status_message_ = "Applied text size.";
                RefreshBanner();
            }
            return;
        }

        case kDownloadablePictureConfirmedMessage: {
            const char* value = nullptr;
            if (message->FindString("value", &value) == B_OK && value != nullptr && editor_host_ != nullptr &&
                editor_host_->InsertDownloadablePicture(TrimWhitespace(value), "")) {
                HandleBodyEdited();
                transient_status_message_ = "Inserted downloadable picture.";
                RefreshBanner();
            }
            return;
        }

        case kInsertPictureMessage:
            HandleInsertPicture();
            return;

        case kRemoveAttachmentMessage:
            HandleRemoveAttachment();
            return;

        case kAddAttachmentSelected: {
            std::string errors;
            bool added = false;
            entry_ref ref;
            for (int32 index = 0; message->FindRef("refs", index, &ref) == B_OK; ++index) {
                BEntry entry(&ref, true);
                BPath path;
                if (entry.GetPath(&path) != B_OK) {
                    continue;
                }
                std::string error_message;
                if (controller_->AddAttachment({path.Leaf(), path.Path(), "", 0, "", false}, &error_message)) {
                    added = true;
                    continue;
                }
                if (!errors.empty()) {
                    errors += '\n';
                }
                errors += error_message;
            }
            if (added) {
                transient_status_message_ = "Attachment list updated.";
                RefreshFromController(false);
                utility_tabs_->Select(1);
            }
            if (!errors.empty()) {
                BAlert("add-attachment-alert", errors.c_str(), "OK")->Go();
            }
            return;
        }

        case kInsertPictureSelected: {
            std::string errors;
            bool inserted = false;
            entry_ref ref;
            for (int32 index = 0; message->FindRef("refs", index, &ref) == B_OK; ++index) {
                BEntry entry(&ref, true);
                BPath path;
                if (entry.GetPath(&path) != B_OK) {
                    continue;
                }
                const std::filesystem::path source_path = path.Path();
                const std::string mime_type = MimeTypeForImagePath(source_path);
                if (mime_type.empty()) {
                    if (!errors.empty()) {
                        errors += '\n';
                    }
                    errors += "Unsupported picture type: " + source_path.filename().string();
                    continue;
                }
                const std::string content_id =
                    "inline-" + compose_cache_key_ + "-" + std::to_string(system_time()) + "-" +
                    std::to_string(index);
                std::string error_message;
                if (!controller_->AddAttachment({source_path.filename().string(),
                                                 source_path,
                                                 mime_type,
                                                 0,
                                                 content_id,
                                                 true},
                                                &error_message)) {
                    if (!errors.empty()) {
                        errors += '\n';
                    }
                    errors += error_message;
                    continue;
                }
                const std::string html = "<img src=\"cid:" + content_id + "\" alt=\"" +
                                         source_path.filename().string() + "\" />";
                if (!surface_->ReplaceSelection(html)) {
                    if (!errors.empty()) {
                        errors += '\n';
                    }
                    errors += "Unable to insert picture markup for " + source_path.filename().string() + ".";
                    continue;
                }
                inserted = true;
            }
            if (inserted) {
                transient_status_message_ = "Inserted picture.";
                HandleBodyEdited();
                utility_tabs_->Select(1);
            }
            if (!errors.empty()) {
                BAlert("insert-picture-alert", errors.c_str(), "OK")->Go();
            }
            return;
        }

        case kStationeryMessage: {
            const char* name = nullptr;
            if (message->FindString("name", &name) == B_OK && name != nullptr &&
                controller_->ApplyStationery(name)) {
                transient_status_message_ = "Applied stationery.";
                RefreshFromController(true);
            }
            return;
        }

        case kSignatureMessage: {
            const char* name = nullptr;
            if (message->FindString("name", &name) == B_OK && name != nullptr &&
                controller_->ApplySignature(name)) {
                transient_status_message_ = "Updated signature.";
                RefreshFromController(true);
            }
            return;
        }

        case kPersonaMessage: {
            const char* persona_id = nullptr;
            if (message->FindString("persona_id", &persona_id) == B_OK && persona_id != nullptr &&
                controller_->UpdateHeader(ComposeHeaderField::kFromPersona, persona_id)) {
                transient_status_message_.clear();
                RefreshFromController(false);
            }
            return;
        }

        case kSurfaceMessage: {
            bool html_surface = false;
            if (message->FindBool("html_surface", &html_surface) != B_OK) {
                return;
            }
            if (!html_surface && hermes::RequiresHtmlSurface(controller_->Snapshot().body)) {
                transient_status_message_ = "This message requires the HTML surface.";
                RefreshBanner();
                return;
            }
            RecreateEditorSurface(controller_->Snapshot(), html_surface);
            transient_status_message_.clear();
            RefreshFromController(true);
            return;
        }

        case kPriorityMessage:
        case kEncodingMessage:
        case kComposeOptionChangedMessage:
            UpdateControllerFromControls();
            return;

        case kDiagnosticMessage:
            NavigateToDiagnostic(diagnostics_list_->CurrentSelection());
            return;

        case kHeaderModifiedMessage:
            UpdateControllerFromControls();
            return;

        case kSaveAsStationeryConfirmedMessage: {
            const char* name = nullptr;
            if (message->FindString("name", &name) == B_OK && name != nullptr) {
                auto snapshot = controller_->Snapshot();
                StationeryTemplate stationery;
                stationery.name = TrimWhitespace(name);
                stationery.headers = snapshot.headers;
                stationery.body = snapshot.body;
                stationery.persona = snapshot.headers.from_persona;
                stationery.signature_name = snapshot.signature_name;
                if (stationery.name.empty()) {
                    BAlert("stationery-name-alert", "Please enter a stationery name.", "OK")->Go();
                    return;
                }
                std::string error_message;
                if (!shell_host_.Stationery().SaveTemplate(stationery, &error_message)) {
                    BAlert("stationery-save-alert",
                           error_message.empty() ? "Unable to save stationery."
                                                 : error_message.c_str(),
                           "OK")
                        ->Go();
                    return;
                }
                shell_host_.Stationery().Discover(shell_host_.Stationery().RootDirectory(), &error_message);
                shell_host_.ReloadWorkspace();
                transient_status_message_ = "Saved as stationery.";
                PopulateMenus();
                RefreshFromController(false);
            }
            return;
        }

        case kFindConfirmedMessage: {
            const char* query = nullptr;
            if (message->FindString("query", &query) == B_OK && query != nullptr) {
                RunFindQuery(query, false);
            }
            return;
        }

        case kFindClosedMessage:
            RestoreFindFocus();
            return;

        case kCancelFormatPainterMessage:
            if (format_painter_armed_) {
                CancelFormatPainter();
            }
            return;

        case B_REFS_RECEIVED:
            if (HandleAttachmentRefs(message)) {
                return;
            }
            break;

        case kIdleTickMessage: {
            UpdateControllerFromControls();
            const bigtime_t idle_micros = std::max<bigtime_t>(0, system_time() - last_edit_time_);
            const auto checks =
                controller_->ServiceAutomaticChecks(std::chrono::milliseconds(idle_micros / 1000));
            if (checks.spell_checked || checks.mood_checked || checks.boss_protector_checked) {
                transient_status_message_.clear();
                RefreshFromController(true);
            }
            return;
        }

        default:
            BWindow::MessageReceived(message);
            return;
    }
}

bool HaikuComposeWindow::QuitRequested() {
    PersistGuiPreferences();
    Hide();
    return true;
}

void HaikuComposeWindow::WindowActivated(bool active) {
    BWindow::WindowActivated(active);
    if (!active && format_painter_armed_) {
        CancelFormatPainter(true);
    }
}

void HaikuComposeWindow::MenusBeginning() {
    BWindow::MenusBeginning();
    if (find_again_item_ != nullptr) {
        find_again_item_->SetEnabled(HasSharedFindQuery());
    }
    if (show_headers_item_ != nullptr) {
        show_headers_item_->SetMarked(headers_visible_);
    }
    if (insert_picture_item_ != nullptr) {
        insert_picture_item_->SetEnabled(UsingHtmlSurface());
    }
    if (print_item_ != nullptr) {
        print_item_->SetEnabled(controller_ != nullptr);
    }
    if (print_one_item_ != nullptr) {
        print_one_item_->SetEnabled(controller_ != nullptr);
    }
    if (print_preview_item_ != nullptr) {
        print_preview_item_->SetEnabled(controller_ != nullptr);
    }
    if (insert_recipients_menu_ != nullptr && insert_recipients_menu_->Superitem() != nullptr) {
        insert_recipients_menu_->Superitem()->SetEnabled(
            ActiveRecipientControl() != nullptr && insert_recipients_menu_->CountItems() > 0);
    }
    RefreshEditorCommandMenus();
}

void HaikuComposeWindow::PopulateMenus() {
    BMenu* persona_menu = persona_field_->Menu();
    persona_menu->RemoveItems(0, persona_menu->CountItems(), true);
    for (const auto& account : shell_host_.Accounts().Accounts()) {
        const std::string label = account.display_name.empty() ? account.id : account.display_name;
        auto* item_message = new BMessage(kPersonaMessage);
        item_message->AddString("persona_id", account.id.c_str());
        persona_menu->AddItem(new BMenuItem(label.c_str(), item_message));
    }

    BMenu* priority_menu = priority_field_->Menu();
    priority_menu->RemoveItems(0, priority_menu->CountItems(), true);
    for (const auto priority : {hermes::ComposePriority::kHighest,
                                hermes::ComposePriority::kHigh,
                                hermes::ComposePriority::kNormal,
                                hermes::ComposePriority::kLow,
                                hermes::ComposePriority::kLowest}) {
        auto* item_message = new BMessage(kPriorityMessage);
        item_message->AddInt32("priority", static_cast<int32>(priority));
        priority_menu->AddItem(new BMenuItem(PriorityLabel(priority), item_message));
    }

    BMenu* surface_menu = surface_field_->Menu();
    surface_menu->RemoveItems(0, surface_menu->CountItems(), true);
    for (const bool html_surface : {false, true}) {
        auto* item_message = new BMessage(kSurfaceMessage);
        item_message->AddBool("html_surface", html_surface);
        surface_menu->AddItem(new BMenuItem(SurfaceLabel(html_surface), item_message));
    }

    BMenu* encoding_menu = encoding_field_->Menu();
    encoding_menu->RemoveItems(0, encoding_menu->CountItems(), true);
    for (const auto encoding : {hermes::AttachmentEncodingMode::kMime,
                                hermes::AttachmentEncodingMode::kBinHex,
                                hermes::AttachmentEncodingMode::kUuencode}) {
        auto* item_message = new BMessage(kEncodingMessage);
        item_message->AddInt32("encoding", static_cast<int32>(encoding));
        encoding_menu->AddItem(new BMenuItem(EncodingLabel(encoding), item_message));
    }

    BMenu* stationery_menu = stationery_field_->Menu();
    stationery_menu->RemoveItems(0, stationery_menu->CountItems(), true);
    for (const auto& stationery : controller_->AvailableStationery()) {
        auto* item_message = new BMessage(kStationeryMessage);
        item_message->AddString("name", stationery.name.c_str());
        stationery_menu->AddItem(new BMenuItem(stationery.name.c_str(), item_message));
    }

    BMenu* signature_menu = signature_field_->Menu();
    signature_menu->RemoveItems(0, signature_menu->CountItems(), true);
    for (const auto& signature : controller_->AvailableSignatures()) {
        auto* item_message = new BMessage(kSignatureMessage);
        item_message->AddString("name", signature.name.c_str());
        signature_menu->AddItem(new BMenuItem(signature.name.c_str(), item_message));
    }

    if (insert_recipients_menu_ != nullptr) {
        insert_recipients_menu_->RemoveItems(0, insert_recipients_menu_->CountItems(), true);
        for (const auto& entry : shell_host_.Nicknames().Entries()) {
            auto* message = new BMessage(kDynamicInsertRecipientMessage);
            message->AddString("nickname_name", entry.nickname.c_str());
            insert_recipients_menu_->AddItem(new BMenuItem(entry.nickname.c_str(), message));
        }
        if (insert_recipients_menu_->CountItems() == 0) {
            auto* empty = new BMenuItem("(No nicknames)", nullptr);
            empty->SetEnabled(false);
            insert_recipients_menu_->AddItem(empty);
        }
    }
}

void HaikuComposeWindow::LoadMessage(const ComposeMessage& message) {
    const bool switched_surface =
        controller_ == nullptr || PreferHtmlSurfaceForDocument(message.body) != UsingHtmlSurface();
    if (switched_surface) {
        RecreateEditorSurface(message, PreferHtmlSurfaceForDocument(message.body));
    } else {
        controller_->Load(message);
    }
    SyncHeaderControlsFromController();
    SyncMenuFieldsFromController();
    last_find_query_.clear();
    last_find_end_offset_ = 0;
    RefreshFromController(true);
    ApplyInitialFocus();
}

void HaikuComposeWindow::UpdateControllerFromControls() {
    bool changed = false;

    const auto update = [&](ComposeHeaderField field, BTextControl* control) {
        const std::string text = ControlValue(control);
        if (controller_->HeaderValue(field) != text) {
            controller_->UpdateHeader(field, text);
            changed = true;
        }
    };

    update(ComposeHeaderField::kTo, to_control_);
    update(ComposeHeaderField::kCc, cc_control_);
    update(ComposeHeaderField::kBcc, bcc_control_);
    update(ComposeHeaderField::kSubject, subject_control_);
    update(ComposeHeaderField::kReplyTo, reply_to_control_);

    hermes::ComposeOptions options = controller_->Options();
    if (BMenu* menu = priority_field_->Menu()) {
        if (BMenuItem* item = menu->FindMarked()) {
            int32 value = static_cast<int32>(options.priority);
            if (item->Message() != nullptr && item->Message()->FindInt32("priority", &value) == B_OK) {
                options.priority = static_cast<hermes::ComposePriority>(value);
            }
        }
    }
    if (BMenu* menu = encoding_field_->Menu()) {
        if (BMenuItem* item = menu->FindMarked()) {
            int32 value = static_cast<int32>(options.attachment_encoding);
            if (item->Message() != nullptr && item->Message()->FindInt32("encoding", &value) == B_OK) {
                options.attachment_encoding = static_cast<hermes::AttachmentEncodingMode>(value);
            }
        }
    }
    options.quoted_printable =
        quoted_printable_box_ != nullptr && quoted_printable_box_->Value() == B_CONTROL_ON;
    options.text_as_document =
        text_as_document_box_ != nullptr && text_as_document_box_->Value() == B_CONTROL_ON;
    options.word_wrap = word_wrap_box_ != nullptr && word_wrap_box_->Value() == B_CONTROL_ON;
    options.tabs_in_body =
        tabs_in_body_box_ != nullptr && tabs_in_body_box_->Value() == B_CONTROL_ON;
    options.keep_copies = keep_copies_box_ != nullptr && keep_copies_box_->Value() == B_CONTROL_ON;
    options.request_read_receipt =
        return_receipt_box_ != nullptr && return_receipt_box_->Value() == B_CONTROL_ON;
    if (controller_->Options().priority != options.priority ||
        controller_->Options().attachment_encoding != options.attachment_encoding ||
        controller_->Options().quoted_printable != options.quoted_printable ||
        controller_->Options().text_as_document != options.text_as_document ||
        controller_->Options().word_wrap != options.word_wrap ||
        controller_->Options().tabs_in_body != options.tabs_in_body ||
        controller_->Options().keep_copies != options.keep_copies ||
        controller_->Options().request_read_receipt != options.request_read_receipt) {
        controller_->UpdateOptions(options);
        changed = true;
    }

    if (changed) {
        transient_status_message_.clear();
        ResetIdleClock();
        RefreshDiagnostics();
        RefreshBanner();
    }
}

void HaikuComposeWindow::SyncHeaderControlsFromController() {
    to_control_->SetText(controller_->HeaderValue(ComposeHeaderField::kTo).c_str());
    cc_control_->SetText(controller_->HeaderValue(ComposeHeaderField::kCc).c_str());
    bcc_control_->SetText(controller_->HeaderValue(ComposeHeaderField::kBcc).c_str());
    subject_control_->SetText(controller_->HeaderValue(ComposeHeaderField::kSubject).c_str());
    reply_to_control_->SetText(controller_->HeaderValue(ComposeHeaderField::kReplyTo).c_str());
}

void HaikuComposeWindow::SyncMenuFieldsFromController() {
    const ComposeMessage snapshot = controller_->Snapshot();
    auto mark_item = [](BMenu* menu, std::string_view name) {
        if (menu == nullptr) {
            return;
        }
        for (int32 index = 0; index < menu->CountItems(); ++index) {
            if (BMenuItem* item = menu->ItemAt(index)) {
                item->SetMarked(name == item->Label());
            }
        }
    };

    if (BMenu* persona_menu = persona_field_->Menu()) {
        for (int32 index = 0; index < persona_menu->CountItems(); ++index) {
            if (BMenuItem* item = persona_menu->ItemAt(index)) {
                const char* persona_id = nullptr;
                const bool marked = item->Message() != nullptr &&
                                    item->Message()->FindString("persona_id", &persona_id) == B_OK &&
                                    persona_id != nullptr && snapshot.headers.from_persona == persona_id;
                item->SetMarked(marked);
            }
        }
    }

    if (BMenu* priority_menu = priority_field_->Menu()) {
        for (int32 index = 0; index < priority_menu->CountItems(); ++index) {
            if (BMenuItem* item = priority_menu->ItemAt(index)) {
                int32 value = 0;
                const bool marked = item->Message() != nullptr &&
                                    item->Message()->FindInt32("priority", &value) == B_OK &&
                                    snapshot.options.priority == static_cast<hermes::ComposePriority>(value);
                item->SetMarked(marked);
            }
        }
    }

    if (BMenu* surface_menu = surface_field_->Menu()) {
        for (int32 index = 0; index < surface_menu->CountItems(); ++index) {
            if (BMenuItem* item = surface_menu->ItemAt(index)) {
                bool html_surface = false;
                const bool marked = item->Message() != nullptr &&
                                    item->Message()->FindBool("html_surface", &html_surface) == B_OK &&
                                    html_surface == UsingHtmlSurface();
                item->SetMarked(marked);
            }
        }
    }

    if (BMenu* encoding_menu = encoding_field_->Menu()) {
        for (int32 index = 0; index < encoding_menu->CountItems(); ++index) {
            if (BMenuItem* item = encoding_menu->ItemAt(index)) {
                int32 value = 0;
                const bool marked = item->Message() != nullptr &&
                                    item->Message()->FindInt32("encoding", &value) == B_OK &&
                                    snapshot.options.attachment_encoding ==
                                        static_cast<hermes::AttachmentEncodingMode>(value);
                item->SetMarked(marked);
            }
        }
    }

    mark_item(stationery_field_->Menu(), snapshot.stationery_name);
    mark_item(signature_field_->Menu(), snapshot.signature_name);
    SetCheckbox(quoted_printable_box_, snapshot.options.quoted_printable);
    SetCheckbox(text_as_document_box_, snapshot.options.text_as_document);
    SetCheckbox(word_wrap_box_, snapshot.options.word_wrap);
    SetCheckbox(tabs_in_body_box_, snapshot.options.tabs_in_body);
    SetCheckbox(keep_copies_box_, snapshot.options.keep_copies);
    SetCheckbox(return_receipt_box_, snapshot.options.request_read_receipt);
}

void HaikuComposeWindow::RefreshDiagnostics() {
    diagnostics_list_->MakeEmpty();
    const auto& diagnostics = controller_->Diagnostics();
    for (const auto& diagnostic : diagnostics) {
        diagnostics_list_->AddItem(new BStringItem(DiagnosticSummary(diagnostic).c_str()));
    }
}

void HaikuComposeWindow::RefreshAttachments() {
    attachment_list_->MakeEmpty();
    for (const auto& attachment : controller_->Attachments()) {
        attachment_list_->AddItem(new BStringItem(AttachmentSummary(attachment).c_str()));
    }
}

void HaikuComposeWindow::RefreshBanner() {
    const auto banner = controller_->StatusBanner();
    if (banner) {
        transient_status_message_.clear();
        banner_view_->SetText((banner->title + ": " + banner->message).c_str());
        return;
    }
    if (!transient_status_message_.empty()) {
        banner_view_->SetText(transient_status_message_.c_str());
        return;
    }
    if (controller_->IsDirty()) {
        banner_view_->SetText("Draft has unsaved changes.");
        return;
    }
    banner_view_->SetText("Compose ready.");
}

void HaikuComposeWindow::RefreshMoodIndicator() {
    if (mood_view_ == nullptr || controller_ == nullptr) {
        return;
    }
    const auto mood = controller_->RunMoodWatch();
    if (!mood.available) {
        mood_view_->SetText("MoodWatch: unavailable");
        return;
    }
    std::ostringstream label;
    label << "MoodWatch: " << mood.score;
    if (mood.score >= 4) {
        label << " (on fire)";
    } else if (mood.score >= 3) {
        label << " (probably offensive)";
    } else if (mood.score >= 2) {
        label << " (might offend)";
    } else {
        label << " (clear)";
    }
    mood_view_->SetText(label.str().c_str());
}

void HaikuComposeWindow::RefreshFromController(bool reload_editor) {
    SyncHeaderControlsFromController();
    SyncMenuFieldsFromController();
    RefreshDiagnostics();
    RefreshAttachments();
    RefreshBanner();
    RefreshMoodIndicator();
    if (editor_host_ == nullptr) {
        return;
    }
    if (reload_editor) {
        editor_host_->ReloadFromSurface();
        editor_host_->ScrollSelectionIntoView();
    } else {
        editor_host_->InvalidateEditor();
    }
}

void HaikuComposeWindow::ResetIdleClock() {
    last_edit_time_ = system_time();
}

void HaikuComposeWindow::HandleBodyEdited() {
    if (format_painter_armed_ && !applying_format_painter_) {
        CancelFormatPainter(true);
    }
    transient_status_message_.clear();
    controller_->MarkBodyEdited();
    ResetIdleClock();
    last_find_end_offset_ = 0;
    RefreshFromController(true);
}

void HaikuComposeWindow::HandleAddAttachment() {
    if (!attachment_open_panel_) {
        attachment_open_panel_ = std::make_unique<BFilePanel>(B_OPEN_PANEL,
                                                              new BMessenger(this),
                                                              nullptr,
                                                              B_FILE_NODE,
                                                              true,
                                                              new BMessage(kAddAttachmentSelected),
                                                              nullptr,
                                                              false,
                                                              true);
    }
    attachment_open_panel_->Show();
}

void HaikuComposeWindow::HandleInsertPicture() {
    if (!UsingHtmlSurface()) {
        transient_status_message_ = "Insert Picture is only available on the HTML surface.";
        RefreshBanner();
        return;
    }
    if (!picture_open_panel_) {
        picture_open_panel_ = std::make_unique<BFilePanel>(B_OPEN_PANEL,
                                                           new BMessenger(this),
                                                           nullptr,
                                                           B_FILE_NODE,
                                                           true,
                                                           new BMessage(kInsertPictureSelected),
                                                           nullptr,
                                                           false,
                                                           true);
    }
    picture_open_panel_->Show();
}

bool HaikuComposeWindow::HandleAttachmentRefs(BMessage* message) {
    std::string errors;
    bool added = false;
    entry_ref ref;
    for (int32 index = 0; message->FindRef("refs", index, &ref) == B_OK; ++index) {
        BEntry entry(&ref, true);
        BPath path;
        if (entry.GetPath(&path) != B_OK) {
            continue;
        }
        std::string error_message;
        if (controller_->AddAttachment({path.Leaf(), path.Path(), "", 0, "", false}, &error_message)) {
            added = true;
            continue;
        }
        if (!errors.empty()) {
            errors += '\n';
        }
        errors += error_message;
    }
    if (added) {
        transient_status_message_ = "Attachment list updated.";
        RefreshFromController(false);
        utility_tabs_->Select(1);
    }
    if (!errors.empty()) {
        BAlert("drop-attachment-alert", errors.c_str(), "OK")->Go();
    }
    return added;
}

bool HaikuComposeWindow::HandleClipboardAttachmentPaste() {
    if (be_clipboard == nullptr || !be_clipboard->Lock()) {
        return false;
    }

    BMessage refs_message;
    bool has_refs = false;
    if (BMessage* clipboard_data = be_clipboard->Data()) {
        entry_ref ref;
        for (int32 index = 0; clipboard_data->FindRef("refs", index, &ref) == B_OK; ++index) {
            refs_message.AddRef("refs", &ref);
            has_refs = true;
        }
    }
    be_clipboard->Unlock();

    return has_refs && HandleAttachmentRefs(&refs_message);
}

bool HaikuComposeWindow::HandlePasteAsQuotation() {
    const auto clipboard_text = ClipboardText();
    if (!clipboard_text || clipboard_text->empty()) {
        return false;
    }
    if (!surface_->ReplaceSelection(QuoteText(*clipboard_text))) {
        return false;
    }
    transient_status_message_ = "Pasted clipboard text as quotation.";
    HandleBodyEdited();
    return true;
}

void HaikuComposeWindow::HandleRemoveAttachment() {
    const int32 selection = attachment_list_->CurrentSelection();
    if (selection < 0) {
        return;
    }
    if (controller_->RemoveAttachment(static_cast<std::size_t>(selection))) {
        transient_status_message_ = "Removed selected attachment.";
        RefreshFromController(false);
    }
}

void HaikuComposeWindow::HandleSaveDraft() {
    UpdateControllerFromControls();
    std::string error_message;
    if (!controller_->SaveDraft(shell_host_.Drafts(), &error_message)) {
        BAlert("save-draft-alert",
               error_message.empty() ? "Unable to save draft." : error_message.c_str(),
               "OK")
            ->Go();
        return;
    }

    shell_host_.ReloadWorkspace();
    transient_status_message_ = "Draft saved.";
    RefreshBanner();
}

void HaikuComposeWindow::HandleQueue(bool allow_warnings) {
    UpdateControllerFromControls();
    auto result = hermes::QueueComposeMessage(
        *controller_, shell_host_.Mailboxes(), shell_host_.Messages(), "out", allow_warnings);

    if (!result.queued && !allow_warnings && !result.validation.warnings.empty()) {
        const std::string message =
            "Queue this message?\n\n" + JoinLines(result.validation.warnings);
        if (BAlert("queue-warning-alert", message.c_str(), "Cancel", "Queue")->Go() == 1) {
            HandleQueue(true);
        }
        return;
    }

    if (!result.queued) {
        const std::string error =
            !result.error_message.empty()
                ? result.error_message
                : !result.validation.blocking_errors.empty() ? JoinLines(result.validation.blocking_errors)
                                                             : "Message was not queued.";
        BAlert("queue-alert", error.c_str(), "OK")->Go();
        transient_status_message_.clear();
        RefreshFromController(true);
        return;
    }

    shell_host_.ReloadWorkspace();
    transient_status_message_ = "Message queued in Out.";
    RefreshFromController(true);
}

void HaikuComposeWindow::HandleSendImmediately() {
    UpdateControllerFromControls();
    auto result = hermes::QueueComposeMessage(
        *controller_, shell_host_.Mailboxes(), shell_host_.Messages(), "out", false);

    if (!result.queued && !result.validation.warnings.empty()) {
        const std::string message =
            "Send this message immediately?\n\n" + JoinLines(result.validation.warnings);
        if (BAlert("send-now-warning-alert", message.c_str(), "Cancel", "Send")->Go() == 1) {
            result = hermes::QueueComposeMessage(
                *controller_, shell_host_.Mailboxes(), shell_host_.Messages(), "out", true);
        }
    }

    if (!result.queued) {
        const std::string error =
            !result.error_message.empty()
                ? result.error_message
                : !result.validation.blocking_errors.empty() ? JoinLines(result.validation.blocking_errors)
                                                             : "Message was not sent.";
        BAlert("send-now-alert", error.c_str(), "OK")->Go();
        transient_status_message_.clear();
        RefreshFromController(true);
        return;
    }

    const bool sent = shell_host_.SendQueued();
    shell_host_.ReloadWorkspace();
    transient_status_message_ =
        sent ? "Message sent immediately." : "Message queued, but send did not complete cleanly.";
    RefreshFromController(true);
}

void HaikuComposeWindow::HandleSaveAsStationery() {
    const ComposeMessage snapshot = controller_->Snapshot();
    BMessage payload(kSaveAsStationeryConfirmedMessage);
    auto* prompt = new TextPromptWindow("Save as Stationery",
                                        "Stationery name",
                                        DefaultStationeryNameForMessage(snapshot),
                                        "name",
                                        BMessenger(this),
                                        payload);
    prompt->Show();
}

void HaikuComposeWindow::HandlePrint(bool preview) {
    std::filesystem::path preview_path;
    std::filesystem::path printable_path;
    if (!EnsurePrintArtifacts(&preview_path, &printable_path)) {
        transient_status_message_ = "Printing is not currently available for this message.";
        RefreshBanner();
        return;
    }

    const bool success = preview ? LaunchPath(preview_path) : SendPathToPrinter(printable_path);
    transient_status_message_ =
        success ? (preview ? "Opened compose print preview." : "Sent the compose message to the printer.")
                : (preview ? "Unable to open compose print preview."
                           : "Unable to print the compose message.");
    RefreshBanner();
}

void HaikuComposeWindow::HandleToggleHeaders() {
    if (headers_visible_ && secondary_headers_container_ != nullptr) {
        remembered_secondary_headers_width_ =
            std::max(secondary_headers_container_->Bounds().Width(), 0.0f);
    }
    headers_visible_ = !headers_visible_;
    if (secondary_headers_container_ != nullptr) {
        if (headers_visible_) {
            if (remembered_secondary_headers_width_ > 0.0f) {
                secondary_headers_container_->SetExplicitMinSize(
                    BSize(remembered_secondary_headers_width_, B_SIZE_UNSET));
                secondary_headers_container_->SetExplicitPreferredSize(
                    BSize(remembered_secondary_headers_width_, B_SIZE_UNSET));
            }
            secondary_headers_container_->Show();
        } else {
            secondary_headers_container_->Hide();
        }
        secondary_headers_container_->InvalidateLayout();
    }
    transient_status_message_ =
        headers_visible_ ? "Extended headers are visible." : "Extended headers are hidden.";
    RefreshBanner();
}

void HaikuComposeWindow::HandleInsertSystemConfiguration() {
    const std::string block = BuildSystemConfigurationBlock(controller_->Snapshot(), shell_host_.DataRootPath());
    if (surface_->ReplaceSelection(block)) {
        transient_status_message_ = "Inserted system configuration.";
        HandleBodyEdited();
    }
}

void HaikuComposeWindow::HandleInsertRecipients(std::string_view nickname_name) {
    BTextControl* control = ActiveRecipientControl();
    if (control == nullptr) {
        transient_status_message_ = "Place the caret in To, Cc, Bcc, or Reply-To first.";
        RefreshBanner();
        return;
    }
    const auto entry = shell_host_.Nicknames().FindNickname(nickname_name);
    if (!entry || entry->addresses.empty()) {
        transient_status_message_ = "Unable to resolve the selected nickname.";
        RefreshBanner();
        return;
    }

    std::string updated = ControlValue(control);
    const std::string recipients = JoinCommaList(entry->addresses);
    if (!TrimWhitespace(updated).empty()) {
        if (!updated.empty() && updated.back() != ',' && updated.back() != ';') {
            updated += ", ";
        } else {
            updated += ' ';
        }
    }
    updated += recipients;
    control->SetText(updated.c_str());
    if (control->TextView() != nullptr) {
        control->TextView()->MakeFocus(true);
        const int32 caret = static_cast<int32>(updated.size());
        control->TextView()->Select(caret, caret);
    } else {
        control->MakeFocus(true);
    }
    transient_status_message_ = "Inserted recipients from the selected nickname.";
    UpdateControllerFromControls();
    RefreshBanner();
}

void HaikuComposeWindow::HandleEditorCommand(ComposeEditorCommand command) {
    if (command == ComposeEditorCommand::kFormatPainter) {
        HandleFormatPainterCommand();
        return;
    }
    if (format_painter_armed_ && !applying_format_painter_) {
        CancelFormatPainter(true);
    }
    if (editor_host_ == nullptr || !editor_host_->SupportsCommand(command)) {
        transient_status_message_ = "That formatting command is not available on the active editor surface.";
        RefreshBanner();
        return;
    }

    switch (command) {
        case ComposeEditorCommand::kInsertLink: {
            BMessage payload(kInsertLinkConfirmedMessage);
            auto* prompt = new TextPromptWindow("Insert Link",
                                                "URL",
                                                "https://",
                                                "value",
                                                BMessenger(this),
                                                payload);
            prompt->Show();
            return;
        }
        case ComposeEditorCommand::kTextColor: {
            BMessage payload(kTextColorConfirmedMessage);
            auto* prompt = new TextPromptWindow("Text Color",
                                                "CSS color or #RRGGBB",
                                                "#003366",
                                                "value",
                                                BMessenger(this),
                                                payload);
            prompt->Show();
            return;
        }
        case ComposeEditorCommand::kTextSize: {
            BMessage payload(kTextSizeConfirmedMessage);
            auto* prompt = new TextPromptWindow("Text Size",
                                                "Size (1-7)",
                                                "3",
                                                "value",
                                                BMessenger(this),
                                                payload);
            prompt->Show();
            return;
        }
        case ComposeEditorCommand::kInsertDownloadablePicture: {
            BMessage payload(kDownloadablePictureConfirmedMessage);
            auto* prompt = new TextPromptWindow("Insert Downloadable Picture",
                                                "Image URL",
                                                "https://",
                                                "value",
                                                BMessenger(this),
                                                payload);
            prompt->Show();
            return;
        }
        default:
            break;
    }

    if (!editor_host_->ExecuteCommand(command)) {
        transient_status_message_ = "Unable to apply the requested formatting command.";
        RefreshBanner();
        return;
    }

    HandleBodyEdited();
    switch (command) {
        case ComposeEditorCommand::kInsertHorizontalRule:
            transient_status_message_ = "Inserted a horizontal rule.";
            break;
        case ComposeEditorCommand::kWrapSelection:
            transient_status_message_ = "Wrapped the current selection.";
            break;
        default:
            transient_status_message_ = "Updated the message formatting.";
            break;
    }
    RefreshBanner();
}

void HaikuComposeWindow::HandleFind(bool repeat_last) {
    if (!repeat_last || !HasSharedFindQuery()) {
        find_restore_view_ = CurrentFocus();
        if (find_window_ == nullptr) {
            find_window_ = new HaikuFindWindow(
                BMessenger(this), kFindConfirmedMessage, kFindClosedMessage, "Find");
        }
        if (auto* window = dynamic_cast<HaikuFindWindow*>(find_window_)) {
            window->SetQuery(SharedFindQuery());
            if (window->IsHidden()) {
                window->Show();
            } else {
                window->Activate(true);
            }
            window->FocusQuery();
        }
        return;
    }
    RunFindQuery(SharedFindQuery(), true);
}

void HaikuComposeWindow::RunFindQuery(std::string query, bool repeat_last) {
    query = TrimWhitespace(query);
    if (query.empty()) {
        if (!repeat_last) {
            transient_status_message_ = "Find cancelled.";
            RefreshBanner();
        }
        return;
    }

    const ComposeFindTarget target = repeat_last ? last_find_target_ : ActiveFindTarget();
    const std::size_t start_offset =
        repeat_last && last_find_query_ == query && last_find_target_ == target ? last_find_end_offset_ : 0;

    if (BTextControl* control = ControlForFindTarget(target)) {
        const auto selection = FindNextCaseInsensitive(ControlValue(control), query, start_offset);
        SetSharedFindQuery(query);
        last_find_query_ = query;
        last_find_target_ = target;
        if (!selection) {
            BAlert("compose-find-alert", "No more matches were found.", "OK")->Go();
            last_find_end_offset_ = 0;
            return;
        }
        last_find_end_offset_ = selection->start + selection->length;
        if (control->TextView() != nullptr) {
            control->TextView()->MakeFocus(true);
            control->TextView()->Select(static_cast<int32>(selection->start),
                                        static_cast<int32>(selection->start + selection->length));
        } else {
            control->MakeFocus(true);
        }
        transient_status_message_ = "Found \"" + query + "\".";
        RefreshBanner();
        return;
    }

    const std::string plain_text = surface_->Snapshot().plain_text;
    const auto selection = FindNextCaseInsensitive(plain_text, query, start_offset);
    SetSharedFindQuery(query);
    last_find_query_ = query;
    last_find_target_ = target;
    if (!selection) {
        BAlert("compose-find-alert", "No more matches were found.", "OK")->Go();
        last_find_end_offset_ = 0;
        return;
    }

    last_find_end_offset_ = selection->start + selection->length;
    surface_->RevealSelection(*selection);
    surface_->SetSelection(*selection);
    if (editor_host_ != nullptr) {
        editor_host_->ReloadFromSurface();
        editor_host_->ScrollSelectionIntoView();
        editor_host_->MakeEditorFocus();
    }
    transient_status_message_ = "Found \"" + query + "\".";
    RefreshBanner();
}

void HaikuComposeWindow::RestoreFindFocus() {
    if (find_restore_view_ != nullptr) {
        find_restore_view_->MakeFocus(true);
        return;
    }
    ApplyInitialFocus();
}

void HaikuComposeWindow::NavigateToDiagnostic(int32 index) {
    const auto& diagnostics = controller_->Diagnostics();
    if (index < 0 || static_cast<std::size_t>(index) >= diagnostics.size()) {
        return;
    }

    const auto& diagnostic = diagnostics[static_cast<std::size_t>(index)];
    if (diagnostic.region == ComposeTextRegion::kBody && diagnostic.length != 0) {
        surface_->RevealSelection({diagnostic.offset, diagnostic.length});
        if (editor_host_ != nullptr) {
            editor_host_->ReloadFromSurface();
            editor_host_->ScrollSelectionIntoView();
            editor_host_->MakeEditorFocus();
        }
        return;
    }

    subject_control_->MakeFocus(true);
}

bool HaikuComposeWindow::UsingHtmlSurface() const {
    return dynamic_cast<HaikuWebKitRichTextSurface*>(surface_.get()) != nullptr;
}

void HaikuComposeWindow::RefreshEditorCommandMenus() {
    for (const auto& binding : editor_command_bindings_) {
        if (binding.item == nullptr) {
            continue;
        }
        if (binding.command == ComposeEditorCommand::kFormatPainter) {
            const bool enabled = editor_host_ != nullptr &&
                                 editor_host_->CaptureStyleSnapshot().has_value();
            binding.item->SetEnabled(enabled);
            binding.item->SetMarked(format_painter_armed_);
            continue;
        }
        if (editor_host_ == nullptr || !editor_host_->SupportsCommand(binding.command)) {
            binding.item->SetEnabled(false);
            binding.item->SetMarked(false);
            continue;
        }
        const auto state = editor_host_->CommandState(binding.command);
        binding.item->SetEnabled(state.enabled);
        binding.item->SetMarked(state.checked && !state.indeterminate);
    }
}

void HaikuComposeWindow::RecreateEditorSurface(hermes::ComposeMessage message, bool prefer_html_surface) {
    if (!prefer_html_surface && hermes::RequiresHtmlSurface(message.body)) {
        prefer_html_surface = true;
    }

    if (editor_host_ != nullptr && editor_host_->RootView() != nullptr && editor_host_->RootView()->Parent() != nullptr) {
        editor_host_->RootView()->RemoveSelf();
    }
    editor_host_.reset();
    surface_.reset();
    controller_.reset();

    if (prefer_html_surface) {
        auto webkit_surface = std::make_unique<HaikuWebKitRichTextSurface>(
            shell_host_.Runtime(), shell_host_.DataRootPath() / "Cache" / "WebKit", compose_cache_key_);
        auto webkit_host = CreateWebKitComposeEditorHost(*webkit_surface);
        webkit_host->SetChangeCallback([this]() { HandleBodyEdited(); });
        webkit_host->SetSelectionChangeCallback([this]() { HandleEditorSelectionChanged(); });
        webkit_host->SetTabNavigationCallback(
            [this](bool shift) { return FocusNextComposeField(shift); });
        surface_ = std::move(webkit_surface);
        editor_host_ = std::move(webkit_host);
    } else {
        auto paige_surface = std::make_unique<hermes::PaigeRichTextSurface>(shell_host_.Runtime());
        auto paige_host = CreatePaigeEditorHost(*paige_surface);
        paige_host->SetChangeCallback([this]() { HandleBodyEdited(); });
        paige_host->SetSelectionChangeCallback([this]() { HandleEditorSelectionChanged(); });
        paige_host->SetTabNavigationCallback(
            [this](bool shift) { return FocusNextComposeField(shift); });
        surface_ = std::move(paige_surface);
        editor_host_ = std::move(paige_host);
    }

    controller_ = std::make_unique<hermes::ComposeController>(*surface_,
                                                              spell_service_.get(),
                                                              mood_watch_analyzer_.get(),
                                                              &shell_host_.Nicknames(),
                                                              &shell_host_.Stationery(),
                                                              &shell_host_.Signatures());
    (void)controller_->Load(message);
    headers_visible_ = true;
    if (secondary_headers_container_ != nullptr) {
        secondary_headers_container_->Show();
    }

    if (editor_root_container_ != nullptr && editor_host_ != nullptr && editor_host_->RootView() != nullptr) {
        if (editor_root_container_->GroupLayout() != nullptr) {
            editor_root_container_->GroupLayout()->AddView(editor_host_->RootView());
        }
        editor_root_container_->InvalidateLayout();
    }
    CancelFormatPainter(true);
}

void HaikuComposeWindow::HandleFormatPainterCommand() {
    if (editor_host_ == nullptr) {
        transient_status_message_ = "That formatting command is not available on the active editor surface.";
        RefreshBanner();
        return;
    }

    if (format_painter_armed_) {
        CancelFormatPainter();
        return;
    }

    format_painter_snapshot_ = editor_host_->CaptureStyleSnapshot();
    if (!format_painter_snapshot_.has_value()) {
        transient_status_message_ = "Format Painter is not available on the active editor surface.";
        RefreshBanner();
        return;
    }

    format_painter_armed_ = true;
    ignore_next_format_painter_selection_change_ = true;
    transient_status_message_ = "Format Painter armed. Click or select the next target to apply the copied formatting.";
    RefreshEditorCommandMenus();
    RefreshBanner();
}

void HaikuComposeWindow::HandleEditorSelectionChanged() {
    if (!format_painter_armed_ || !format_painter_snapshot_.has_value() || editor_host_ == nullptr) {
        return;
    }
    if (ignore_next_format_painter_selection_change_) {
        ignore_next_format_painter_selection_change_ = false;
        return;
    }

    applying_format_painter_ = true;
    const bool applied = editor_host_->ApplyStyleSnapshot(*format_painter_snapshot_);
    applying_format_painter_ = false;
    CancelFormatPainter(true);
    transient_status_message_ = applied ? "Applied copied formatting." : "Unable to apply the copied formatting.";
    if (applied) {
        HandleBodyEdited();
    } else {
        RefreshEditorCommandMenus();
        RefreshBanner();
    }
}

void HaikuComposeWindow::CancelFormatPainter(bool preserve_status) {
    const bool was_armed = format_painter_armed_;
    format_painter_armed_ = false;
    ignore_next_format_painter_selection_change_ = false;
    format_painter_snapshot_.reset();
    RefreshEditorCommandMenus();
    if (!preserve_status && was_armed) {
        transient_status_message_ = "Format Painter cancelled.";
        RefreshBanner();
    }
}

bool HaikuComposeWindow::EnsurePrintArtifacts(std::filesystem::path* preview_path,
                                             std::filesystem::path* printable_path) const {
    if (controller_ == nullptr) {
        return false;
    }

    const ComposeMessage snapshot = controller_->Snapshot();
    hermes::RichTextDocument prepared_body =
        hermes::PrepareRichTextDocumentForPersistence(surface_ != nullptr ? surface_->Snapshot() : snapshot.body);
    if (prepared_body.html_fragment.empty()) {
        if (!prepared_body.plain_text.empty()) {
            prepared_body.html_fragment =
                "<pre>" + EscapeHtmlText(prepared_body.plain_text) + "</pre>";
        } else if (!prepared_body.rtf_fragment.empty()) {
            const std::string stripped = hermes::StripRtf(prepared_body.rtf_fragment);
            prepared_body.html_fragment = "<pre>" + EscapeHtmlText(stripped) + "</pre>";
            prepared_body.plain_text = stripped;
        }
    }

    std::string preview_document = prepared_body.html_fragment;
    if (!LooksLikeHtmlDocument(preview_document)) {
        preview_document = WrapPrintHtmlDocument(
            "Compose Print Preview",
            BuildComposePrintHeaderHtml(snapshot) + prepared_body.html_fragment);
    }

    std::string printable_text = BuildComposePrintHeaderText(snapshot);
    const std::string body_printable_text = hermes::StripHtml(prepared_body.html_fragment);
    if (!printable_text.empty()) {
        printable_text.push_back('\n');
    }
    printable_text += body_printable_text;
    if (printable_text.empty()) {
        return false;
    }

    const auto render_directory =
        shell_host_.DataRootPath() / "Cache" / "WebKit" / "compose-print" / compose_cache_key_;
    std::error_code mkdir_error;
    std::filesystem::create_directories(render_directory, mkdir_error);
    if (mkdir_error) {
        return false;
    }

    const auto print_path = render_directory / "print-compose.txt";
    {
        std::ofstream output(print_path, std::ios::binary);
        if (!output.is_open()) {
            return false;
        }
        output << printable_text;
    }

    const auto preview_document_path = render_directory / "print-preview.html";
    {
        std::ofstream output(preview_document_path, std::ios::binary);
        if (!output.is_open()) {
            return false;
        }
        output << preview_document;
    }

    if (preview_path != nullptr) {
        *preview_path = preview_document_path;
    }
    if (printable_path != nullptr) {
        *printable_path = print_path;
    }
    return true;
}

void HaikuComposeWindow::PersistGuiPreferences() {
    if (utility_tabs_ != nullptr) {
        gui_preferences_.compose_utility_pane_selected_tab = utility_tabs_->Selection();
        gui_preferences_.compose_utility_pane_height =
            std::max(96, static_cast<int>(utility_tabs_->Frame().Height()));
    }
    ApplyGuiPreferencesToSettings(gui_preferences_, shell_host_.Settings());
    std::string ignored;
    shell_host_.PersistSettings(&ignored);
}

void HaikuComposeWindow::RebuildToolbar() {
    if (toolbar_view_ == nullptr) {
        return;
    }
    const auto actions = ComposeToolbarActionSpecs(shell_host_);
    const auto allowed_entries = ToolbarAllowedEntries(actions);
    const auto configuration = ParseToolbarConfiguration(gui_preferences_.compose_toolbar_layout,
                                                         allowed_entries,
                                                         ComposeToolbarDefaultEntries());
    PopulateToolbar(*static_cast<BToolBar*>(toolbar_view_),
                    this,
                    actions,
                    configuration,
                    gui_preferences_.show_toolbar_tips,
                    gui_preferences_.show_toolbar_large_buttons);
}

HaikuComposeWindow::ComposeFindTarget HaikuComposeWindow::ActiveFindTarget() const {
    BView* focus = CurrentFocus();
    const auto matches = [focus](BTextControl* control) {
        return control != nullptr &&
               (focus == control || (control->TextView() != nullptr && focus == control->TextView()));
    };
    if (matches(to_control_)) {
        return ComposeFindTarget::kTo;
    }
    if (matches(cc_control_)) {
        return ComposeFindTarget::kCc;
    }
    if (matches(bcc_control_)) {
        return ComposeFindTarget::kBcc;
    }
    if (matches(subject_control_)) {
        return ComposeFindTarget::kSubject;
    }
    if (matches(reply_to_control_)) {
        return ComposeFindTarget::kReplyTo;
    }
    return ComposeFindTarget::kBody;
}

BTextControl* HaikuComposeWindow::ControlForFindTarget(ComposeFindTarget target) const {
    switch (target) {
        case ComposeFindTarget::kTo:
            return to_control_;
        case ComposeFindTarget::kCc:
            return cc_control_;
        case ComposeFindTarget::kBcc:
            return bcc_control_;
        case ComposeFindTarget::kSubject:
            return subject_control_;
        case ComposeFindTarget::kReplyTo:
            return reply_to_control_;
        case ComposeFindTarget::kBody:
            return nullptr;
    }
    return nullptr;
}

BTextControl* HaikuComposeWindow::ActiveRecipientControl() const {
    BView* focus = CurrentFocus();
    const auto matches = [focus](BTextControl* control) {
        return control != nullptr &&
               (focus == control || (control->TextView() != nullptr && focus == control->TextView()));
    };
    if (matches(to_control_)) {
        return to_control_;
    }
    if (matches(cc_control_)) {
        return cc_control_;
    }
    if (matches(bcc_control_)) {
        return bcc_control_;
    }
    if (matches(reply_to_control_)) {
        return reply_to_control_;
    }
    return nullptr;
}

bool HaikuComposeWindow::FocusNextComposeField(bool shift) {
    if (controller_ == nullptr || controller_->Options().tabs_in_body) {
        return false;
    }

    BTextControl* target = shift ? (headers_visible_ ? reply_to_control_ : subject_control_) : to_control_;
    if (target != nullptr && target->TextView() != nullptr) {
        target->TextView()->MakeFocus(true);
        return true;
    }
    if (target != nullptr) {
        target->MakeFocus(true);
        return true;
    }
    return false;
}

void HaikuComposeWindow::ApplyInitialFocus() {
    if (TrimWhitespace(ControlValue(to_control_)).empty()) {
        if (to_control_ != nullptr && to_control_->TextView() != nullptr) {
            to_control_->TextView()->MakeFocus(true);
            return;
        }
    }
    if (TrimWhitespace(ControlValue(subject_control_)).empty()) {
        if (subject_control_ != nullptr && subject_control_->TextView() != nullptr) {
            subject_control_->TextView()->MakeFocus(true);
            return;
        }
    }
    if (editor_host_ != nullptr) {
        editor_host_->MakeEditorFocus();
    }
}

void HaikuComposeWindow::ApplyUtilityPaneSizing() {
    if (utility_container_ == nullptr || utility_tabs_ == nullptr) {
        return;
    }
    utility_container_->SetExplicitMinSize(
        BSize(B_SIZE_UNSET, std::max(96, gui_preferences_.compose_utility_pane_height)));
    if (utility_tabs_->CountTabs() > 0) {
        const int32 max_index = std::max<int32>(0, utility_tabs_->CountTabs() - 1);
        const int32 selection = std::max<int32>(
            0, std::min<int32>(gui_preferences_.compose_utility_pane_selected_tab, max_index));
        utility_tabs_->Select(selection);
    }
}

}  // namespace hemera::haiku
