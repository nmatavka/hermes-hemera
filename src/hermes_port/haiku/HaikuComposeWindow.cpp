#include "HaikuComposeWindow.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>

#include <Alert.h>
#include <Application.h>
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
#include <ScrollView.h>
#include <SplitView.h>
#include <StringItem.h>
#include <StringView.h>
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
constexpr uint32_t kIdleTickMessage = 'idlt';
constexpr uint32_t kDiagnosticMessage = 'diag';
constexpr uint32_t kHeaderModifiedMessage = 'hedr';

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

}  // namespace

HaikuComposeWindow::HaikuComposeWindow(HaikuShellHost& shell_host, const ComposeMessage& message)
    : BWindow(BRect(140, 140, 1180, 920),
              "Compose Message",
              B_TITLED_WINDOW,
              B_ASYNCHRONOUS_CONTROLS),
      shell_host_(shell_host),
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

    to_control_ = new BTextControl("to-control", "To", "", nullptr);
    cc_control_ = new BTextControl("cc-control", "Cc", "", nullptr);
    bcc_control_ = new BTextControl("bcc-control", "Bcc", "", nullptr);
    subject_control_ = new BTextControl("subject-control", "Subject", "", nullptr);
    persona_control_ = new BTextControl("persona-control", "Persona", "", nullptr);
    reply_to_control_ = new BTextControl("reply-to-control", "Reply-To", "", nullptr);
    for (BTextControl* control :
         {to_control_, cc_control_, bcc_control_, subject_control_, persona_control_, reply_to_control_}) {
        control->SetTarget(this);
        control->SetModificationMessage(new BMessage(kHeaderModifiedMessage));
    }

    auto* stationery_menu = new BPopUpMenu("stationery-menu");
    stationery_menu->SetRadioMode(true);
    stationery_menu->SetLabelFromMarked(true);
    stationery_field_ = new BMenuField("stationery-field", "Stationery", stationery_menu);

    auto* signature_menu = new BPopUpMenu("signature-menu");
    signature_menu->SetRadioMode(true);
    signature_menu->SetLabelFromMarked(true);
    signature_field_ = new BMenuField("signature-field", "Signature", signature_menu);

    banner_view_ = new BStringView("compose-banner", "Preparing compose window...");
    diagnostics_list_ = new BListView("compose-diagnostics");
    diagnostics_list_->SetSelectionMessage(new BMessage(kDiagnosticMessage));

    editor_view_ = new PaigeEditorView(*surface_);
    editor_view_->SetChangeCallback([this]() { HandleBodyEdited(); });

    auto* diagnostics_scroll =
        new BScrollView("compose-diagnostics-scroll", diagnostics_list_, 0, false, true);
    auto* editor_scroll = new BScrollView("compose-body-scroll", editor_view_, 0, true, true);

    auto* diagnostics_split = new BSplitView(B_VERTICAL);
    diagnostics_split->AddChild(editor_scroll);
    diagnostics_split->AddChild(diagnostics_scroll);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu_bar)
        .AddGroup(B_VERTICAL, 8)
            .SetInsets(B_USE_WINDOW_SPACING)
            .Add(to_control_)
            .Add(cc_control_)
            .Add(bcc_control_)
            .Add(subject_control_)
            .Add(persona_control_)
            .Add(reply_to_control_)
            .AddGroup(B_HORIZONTAL, 8)
                .Add(stationery_field_)
                .Add(signature_field_)
            .End()
            .Add(banner_view_)
            .Add(diagnostics_split)
        .End();

    PopulateMenus();
    LoadMessage(message);
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
            controller_->CheckDocument();
            RefreshFromController(true);
            return;

        case kIgnoreWordMessage:
            if (controller_->IgnoreCurrentWord()) {
                RefreshFromController(true);
            }
            return;

        case kAddWordMessage:
            if (controller_->AddCurrentWord()) {
                RefreshFromController(true);
            }
            return;

        case kReplaceCurrentMessage: {
            const auto suggestions = controller_->SuggestionsForCurrentIssue();
            if (!suggestions.empty() && controller_->ReplaceCurrent(suggestions.front())) {
                RefreshFromController(true);
            }
            return;
        }

        case kStationeryMessage: {
            const char* name = nullptr;
            if (message->FindString("name", &name) == B_OK && name != nullptr &&
                controller_->ApplyStationery(name)) {
                RefreshFromController(true);
            }
            return;
        }

        case kSignatureMessage: {
            const char* name = nullptr;
            if (message->FindString("name", &name) == B_OK && name != nullptr &&
                controller_->ApplySignature(name)) {
                RefreshFromController(true);
            }
            return;
        }

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
    Hide();
    return true;
}

void HaikuComposeWindow::PopulateMenus() {
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
    update(ComposeHeaderField::kFromPersona, persona_control_);
    update(ComposeHeaderField::kReplyTo, reply_to_control_);

    if (changed) {
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
    persona_control_->SetText(controller_->HeaderValue(ComposeHeaderField::kFromPersona).c_str());
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

    mark_item(stationery_field_->Menu(), snapshot.stationery_name);
    mark_item(signature_field_->Menu(), snapshot.signature_name);
}

void HaikuComposeWindow::RefreshDiagnostics() {
    diagnostics_list_->MakeEmpty();
    const auto& diagnostics = controller_->Diagnostics();
    for (const auto& diagnostic : diagnostics) {
        diagnostics_list_->AddItem(new BStringItem(DiagnosticSummary(diagnostic).c_str()));
    }
}

void HaikuComposeWindow::RefreshBanner() {
    const auto banner = controller_->StatusBanner();
    if (banner) {
        banner_view_->SetText((banner->title + ": " + banner->message).c_str());
        return;
    }

    if (!surface_->IsAvailable()) {
        banner_view_->SetText(
            "Native Paige runtime unavailable; compose is running on the guarded surface fallback.");
        return;
    }

    banner_view_->SetText("Compose ready.");
}

void HaikuComposeWindow::RefreshFromController(bool reload_editor) {
    SyncHeaderControlsFromController();
    SyncMenuFieldsFromController();
    RefreshDiagnostics();
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
    controller_->MarkBodyEdited();
    ResetIdleClock();
    RefreshFromController(true);
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
    banner_view_->SetText("Draft saved.");
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
        RefreshFromController(true);
        return;
    }

    shell_host_.ReloadWorkspace();
    banner_view_->SetText("Message queued in Out.");
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
}  // namespace hermes::haiku_port
