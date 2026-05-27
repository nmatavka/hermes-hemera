#pragma once

#include <memory>
#include <string>

#include <OS.h>
#include <Window.h>

#include "hermes/ComposeMessage.h"
#include "hermes/GuiPreferences.h"

class BListView;
class BFilePanel;
class BCheckBox;
class BMenuField;
class BMessageRunner;
class BStringView;
class BTabView;
class BTextControl;
class BView;

namespace hermes {
class ComposeController;
class FlatFileNicknameStore;
class HunspellSpellService;
class PaigeRichTextSurface;
class TaeMoodWatchAnalyzer;
}

namespace hermes::haiku_port {

class HaikuShellHost;
class PaigeEditorView;

class HaikuComposeWindow final : public BWindow {
public:
    HaikuComposeWindow(HaikuShellHost& shell_host, const ComposeMessage& message);
    ~HaikuComposeWindow() override;

    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;

private:
    void PopulateMenus();
    void LoadMessage(const ComposeMessage& message);
    void UpdateControllerFromControls();
    void SyncHeaderControlsFromController();
    void SyncMenuFieldsFromController();
    void RefreshDiagnostics();
    void RefreshAttachments();
    void RefreshBanner();
    void RefreshFromController(bool reload_editor);
    void ResetIdleClock();
    void HandleBodyEdited();
    void HandleAddAttachment();
    void HandleRemoveAttachment();
    void HandleSaveDraft();
    void HandleQueue(bool allow_warnings);
    void NavigateToDiagnostic(int32 index);
    void PersistGuiPreferences();
    void ApplyUtilityPaneSizing();

    HaikuShellHost& shell_host_;
    GuiPreferences gui_preferences_;
    std::unique_ptr<hermes::PaigeRichTextSurface> surface_;
    std::unique_ptr<hermes::HunspellSpellService> spell_service_;
    std::unique_ptr<hermes::TaeMoodWatchAnalyzer> mood_watch_analyzer_;
    std::unique_ptr<hermes::FlatFileNicknameStore> nickname_store_;
    std::unique_ptr<hermes::ComposeController> controller_;
    BTextControl* to_control_ = nullptr;
    BTextControl* cc_control_ = nullptr;
    BTextControl* bcc_control_ = nullptr;
    BTextControl* subject_control_ = nullptr;
    BTextControl* reply_to_control_ = nullptr;
    BMenuField* persona_field_ = nullptr;
    BMenuField* priority_field_ = nullptr;
    BMenuField* encoding_field_ = nullptr;
    BMenuField* stationery_field_ = nullptr;
    BMenuField* signature_field_ = nullptr;
    BCheckBox* quoted_printable_box_ = nullptr;
    BCheckBox* text_as_document_box_ = nullptr;
    BCheckBox* word_wrap_box_ = nullptr;
    BCheckBox* tabs_in_body_box_ = nullptr;
    BCheckBox* keep_copies_box_ = nullptr;
    BCheckBox* return_receipt_box_ = nullptr;
    BStringView* banner_view_ = nullptr;
    BListView* diagnostics_list_ = nullptr;
    BListView* attachment_list_ = nullptr;
    PaigeEditorView* editor_view_ = nullptr;
    BTabView* utility_tabs_ = nullptr;
    BView* utility_container_ = nullptr;
    std::unique_ptr<BFilePanel> attachment_open_panel_;
    std::unique_ptr<BMessageRunner> idle_runner_;
    bigtime_t last_edit_time_ = 0;
    std::string transient_status_message_;
};

}  // namespace hermes::haiku_port
