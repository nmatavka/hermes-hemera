#include "HaikuComposeWindow.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <Entry.h>
#include <FilePanel.h>
#include <GroupView.h>
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

#include "HaikuShellHost.h"
#include "PaigeEditorView.h"
#include "hermes/ComposeController.h"
#include "hermes/ComposeQueue.h"
#include "hermes/HunspellSpellService.h"
#include "hermes/MoodWatchAnalyzer.h"
#include "hermes/NicknameStore.h"
#include "hermes/PaigeRichTextSurface.h"

namespace hermes::haiku_port {

namespace {

constexpr uint32_t kSaveDraftMessage = 'sdrf';
constexpr uint32_t kQueueMessage = 'queu';
constexpr uint32_t kUndoMessage = 'undo';
constexpr uint32_t kRedoMessage = 'redo';
constexpr uint32_t kCutMessage = 'xcut';
constexpr uint32_t kCopyMessage = 'copy';
constexpr uint32_t kPasteMessage = 'past';
constexpr uint32_t kSelectAllMessage = 'sall';
constexpr uint32_t kCheckDocumentMessage = 'spck';
constexpr uint32_t kIgnoreWordMessage = 'spig';
constexpr uint32_t kAddWordMessage = 'spad';
constexpr uint32_t kReplaceCurrentMessage = 'sprp';
constexpr uint32_t kStationeryMessage = 'stny';
constexpr uint32_t kSignatureMessage = 'sgnt';
constexpr uint32_t kPersonaMessage = 'pers';
constexpr uint32_t kPriorityMessage = 'prio';
constexpr uint32_t kEncodingMessage = 'enco';
constexpr uint32_t kComposeOptionChangedMessage = 'copt';
constexpr uint32_t kIdleTickMessage = 'idlt';
constexpr uint32_t kDiagnosticMessage = 'diag';
constexpr uint32_t kHeaderModifiedMessage = 'hedr';
constexpr uint32_t kAddAttachmentMessage = 'atag';
constexpr uint32_t kRemoveAttachmentMessage = 'atrm';
constexpr uint32_t kAddAttachmentSelected = 'atrs';

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

void SetCheckbox(BCheckBox* checkbox, bool enabled) {
    if (checkbox != nullptr) {
        checkbox->SetValue(enabled ? B_CONTROL_ON : B_CONTROL_OFF);
    }
}

}  // namespace

HaikuComposeWindow::HaikuComposeWindow(HaikuShellHost& shell_host, const ComposeMessage& message)
    : BWindow(BRect(140, 140, 1180, 920),
              "Compose Message",
              B_TITLED_WINDOW,
              B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
      shell_host_(shell_host),
      gui_preferences_(GuiPreferencesFromSettings(shell_host.Settings())),
      surface_(std::make_unique<hermes::PaigeRichTextSurface>(shell_host.Runtime())),
      spell_service_(std::make_unique<hermes::HunspellSpellService>(
          SourceRoot() / "third_party" / "hunspell" / "tests" / "base_utf.aff",
          SourceRoot() / "third_party" / "hunspell" / "tests" / "base_utf.dic")),
      mood_watch_analyzer_(std::make_unique<hermes::TaeMoodWatchAnalyzer>(
          SourceRoot() / "src" / "legacy_transplants" / "tae" / "FlameLex.dat")),
      nickname_store_(std::make_unique<hermes::FlatFileNicknameStore>()),
      controller_(std::make_unique<hermes::ComposeController>(*surface_,
                                                              spell_service_.get(),
                                                              mood_watch_analyzer_.get(),
                                                              nickname_store_.get(),
                                                              &shell_host.Stationery(),
                                                              &shell_host.Signatures())) {
    std::string ignored;
    (void)shell_host_.Runtime().Initialize(&ignored);

    auto* menu_bar = new BMenuBar("compose-menu-bar");
    auto* file_menu = new BMenu("File");
    file_menu->AddItem(new BMenuItem("Save Draft", new BMessage(kSaveDraftMessage)));
    file_menu->AddItem(new BMenuItem("Queue", new BMessage(kQueueMessage)));
    file_menu->AddSeparatorItem();
    file_menu->AddItem(new BMenuItem("Close", new BMessage(B_QUIT_REQUESTED)));
    menu_bar->AddItem(file_menu);

    auto* edit_menu = new BMenu("Edit");
    edit_menu->AddItem(new BMenuItem("Undo", new BMessage(kUndoMessage)));
    edit_menu->AddItem(new BMenuItem("Redo", new BMessage(kRedoMessage)));
    edit_menu->AddSeparatorItem();
    edit_menu->AddItem(new BMenuItem("Cut", new BMessage(kCutMessage)));
    edit_menu->AddItem(new BMenuItem("Copy", new BMessage(kCopyMessage)));
    edit_menu->AddItem(new BMenuItem("Paste", new BMessage(kPasteMessage)));
    edit_menu->AddItem(new BMenuItem("Select All", new BMessage(kSelectAllMessage)));
    menu_bar->AddItem(edit_menu);

    auto* spelling_menu = new BMenu("Spelling");
    spelling_menu->AddItem(new BMenuItem("Check Document", new BMessage(kCheckDocumentMessage)));
    spelling_menu->AddItem(new BMenuItem("Ignore Word", new BMessage(kIgnoreWordMessage)));
    spelling_menu->AddItem(new BMenuItem("Add Word", new BMessage(kAddWordMessage)));
    spelling_menu->AddItem(new BMenuItem("Replace Current", new BMessage(kReplaceCurrentMessage)));
    menu_bar->AddItem(spelling_menu);

    auto* attachments_menu = new BMenu("Attachments");
    attachments_menu->AddItem(new BMenuItem("Add Attachment", new BMessage(kAddAttachmentMessage)));
    attachments_menu->AddItem(new BMenuItem("Remove Attachment", new BMessage(kRemoveAttachmentMessage)));
    menu_bar->AddItem(attachments_menu);

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

    auto* queue_button = new BButton("queue-button", "Queue", new BMessage(kQueueMessage));
    auto* save_draft_button = new BButton("save-draft-button", "Save Draft", new BMessage(kSaveDraftMessage));
    auto* attach_button =
        new BButton("attach-button", "Attach", new BMessage(kAddAttachmentMessage));

    banner_view_ = new BStringView("compose-banner", "Compose ready.");
    diagnostics_list_ = new BListView("compose-diagnostics");
    diagnostics_list_->SetSelectionMessage(new BMessage(kDiagnosticMessage));
    attachment_list_ = new BListView("compose-attachments");

    editor_view_ = new PaigeEditorView(*surface_);
    editor_view_->SetChangeCallback([this]() { HandleBodyEdited(); });

    auto* editor_scroll = new BScrollView("compose-body-scroll", editor_view_, 0, true, true);
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

    auto* editor_split = new BSplitView(B_VERTICAL);
    editor_split->AddChild(editor_scroll);
    editor_split->AddChild(utility_container_);

    auto* primary_headers = new BGroupView(B_VERTICAL);
    BLayoutBuilder::Group<>(primary_headers, B_VERTICAL, 6)
        .Add(to_control_)
        .Add(cc_control_)
        .Add(bcc_control_)
        .Add(subject_control_);

    auto* secondary_headers = new BGroupView(B_VERTICAL);
    BLayoutBuilder::Group<>(secondary_headers, B_VERTICAL, 6)
        .Add(persona_field_)
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

    auto* command_strip = new BGroupView(B_HORIZONTAL, 8);
    BLayoutBuilder::Group<>(command_strip, B_HORIZONTAL, 8)
        .Add(queue_button)
        .Add(save_draft_button)
        .Add(attach_button)
        .AddGlue();

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu_bar)
        .AddGroup(B_VERTICAL, 8)
            .SetInsets(B_USE_WINDOW_SPACING)
            .Add(command_strip)
            .AddGroup(B_HORIZONTAL, 12)
                .Add(primary_headers, 2.4f)
                .Add(secondary_headers, 1.5f)
            .End()
            .Add(option_toggles)
            .Add(banner_view_)
            .Add(editor_split)
        .End();

    PopulateMenus();
    LoadMessage(message);
    ApplyUtilityPaneSizing();
    ResetIdleClock();
    idle_runner_ =
        std::make_unique<BMessageRunner>(BMessenger(this), new BMessage(kIdleTickMessage), 250000, -1);
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
            if (editor_view_->CutSelection()) {
                RefreshFromController(true);
            }
            return;

        case kCopyMessage:
            (void)editor_view_->CopySelection();
            return;

        case kPasteMessage:
            if (editor_view_->Paste()) {
                RefreshFromController(true);
            }
            return;

        case kSelectAllMessage:
            if (editor_view_->SelectAllText()) {
                RefreshFromController(false);
            }
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
}

void HaikuComposeWindow::LoadMessage(const ComposeMessage& message) {
    controller_->Load(message);
    SyncHeaderControlsFromController();
    SyncMenuFieldsFromController();
    RefreshFromController(true);
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

void HaikuComposeWindow::RefreshFromController(bool reload_editor) {
    SyncHeaderControlsFromController();
    SyncMenuFieldsFromController();
    RefreshDiagnostics();
    RefreshAttachments();
    RefreshBanner();
    if (reload_editor) {
        editor_view_->ReloadFromSurface();
        editor_view_->ScrollSelectionIntoView();
    } else {
        editor_view_->Invalidate();
    }
}

void HaikuComposeWindow::ResetIdleClock() {
    last_edit_time_ = system_time();
}

void HaikuComposeWindow::HandleBodyEdited() {
    transient_status_message_.clear();
    controller_->MarkBodyEdited();
    ResetIdleClock();
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

void HaikuComposeWindow::NavigateToDiagnostic(int32 index) {
    const auto& diagnostics = controller_->Diagnostics();
    if (index < 0 || static_cast<std::size_t>(index) >= diagnostics.size()) {
        return;
    }

    const auto& diagnostic = diagnostics[static_cast<std::size_t>(index)];
    if (diagnostic.region == ComposeTextRegion::kBody && diagnostic.length != 0) {
        surface_->RevealSelection({diagnostic.offset, diagnostic.length});
        editor_view_->ReloadFromSurface();
        editor_view_->ScrollSelectionIntoView();
        editor_view_->MakeFocus(true);
        return;
    }

    subject_control_->MakeFocus(true);
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

}  // namespace hermes::haiku_port
