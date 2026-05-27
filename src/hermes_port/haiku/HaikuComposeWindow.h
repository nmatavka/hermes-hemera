#pragma once

#include <memory>

#include <OS.h>
#include <Window.h>

#include "hermes/ComposeMessage.h"

class BListView;
class BMenuField;
class BMessageRunner;
class BStringView;
class BTextControl;

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
    void RefreshBanner();
    void RefreshFromController(bool reload_editor);
    void ResetIdleClock();
    void HandleBodyEdited();
    void HandleSaveDraft();
    void HandleQueue(bool allow_warnings);
    void NavigateToDiagnostic(int32 index);

    HaikuShellHost& shell_host_;
    std::unique_ptr<hermes::PaigeRichTextSurface> surface_;
    std::unique_ptr<hermes::HunspellSpellService> spell_service_;
    std::unique_ptr<hermes::TaeMoodWatchAnalyzer> mood_watch_analyzer_;
    std::unique_ptr<hermes::FlatFileNicknameStore> nickname_store_;
    std::unique_ptr<hermes::ComposeController> controller_;
    BTextControl* to_control_ = nullptr;
    BTextControl* cc_control_ = nullptr;
    BTextControl* bcc_control_ = nullptr;
    BTextControl* subject_control_ = nullptr;
    BTextControl* persona_control_ = nullptr;
    BTextControl* reply_to_control_ = nullptr;
    BMenuField* stationery_field_ = nullptr;
    BMenuField* signature_field_ = nullptr;
    BStringView* banner_view_ = nullptr;
    BListView* diagnostics_list_ = nullptr;
    PaigeEditorView* editor_view_ = nullptr;
    std::unique_ptr<BMessageRunner> idle_runner_;
    bigtime_t last_edit_time_ = 0;
};

}  // namespace hermes::haiku_port
