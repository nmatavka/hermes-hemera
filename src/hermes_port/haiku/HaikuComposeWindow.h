#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <OS.h>
#include <Window.h>

#include "ComposeEditorHost.h"
#include "hermes/ComposeMessage.h"
#include "hermes/GuiPreferences.h"

class BListView;
class BFilePanel;
class BCheckBox;
class BGroupView;
class BMenuField;
class BMessageRunner;
class BStringView;
class BTabView;
class BTextControl;
class BView;

namespace hermes {
class ComposeController;
class HunspellSpellService;
class RichTextSurface;
class TaeMoodWatchAnalyzer;
}

namespace hemera::haiku {

class HaikuShellHost;
class HaikuToolbarCustomizationWindow;

class HaikuComposeWindow final : public BWindow {
public:
    HaikuComposeWindow(HaikuShellHost& shell_host, const ComposeMessage& message);
    ~HaikuComposeWindow() override;

    void MessageReceived(BMessage* message) override;
    void MenusBeginning() override;
    bool QuitRequested() override;
    void WindowActivated(bool active) override;

private:
    enum class ComposeFindTarget {
        kBody,
        kTo,
        kCc,
        kBcc,
        kSubject,
        kReplyTo,
    };

    void PopulateMenus();
    void LoadMessage(const ComposeMessage& message);
    void UpdateControllerFromControls();
    void SyncHeaderControlsFromController();
    void SyncMenuFieldsFromController();
    void RefreshDiagnostics();
    void RefreshAttachments();
    void RefreshBanner();
    void RefreshMoodIndicator();
    void RefreshFromController(bool reload_editor);
    void ResetIdleClock();
    void HandleBodyEdited();
    void HandleAddAttachment();
    void HandleInsertPicture();
    bool HandleAttachmentRefs(BMessage* message);
    void HandleRemoveAttachment();
    void HandleSaveDraft();
    void HandleQueue(bool allow_warnings);
    void HandleSendImmediately();
    void HandleSaveAsStationery();
    void HandlePrint(bool preview);
    void HandleToggleHeaders();
    void HandleInsertSystemConfiguration();
    void HandleInsertRecipients(std::string_view nickname_name);
    void HandleEditorCommand(ComposeEditorCommand command);
    void HandleFormatPainterCommand();
    void HandleFind(bool repeat_last);
    void RunFindQuery(std::string query, bool repeat_last);
    void RestoreFindFocus();
    bool HandleClipboardAttachmentPaste();
    bool HandlePasteAsQuotation();
    void NavigateToDiagnostic(int32 index);
    void PersistGuiPreferences();
    void ApplyUtilityPaneSizing();
    void RebuildToolbar();
    void RefreshEditorCommandMenus();
    void RecreateEditorSurface(hermes::ComposeMessage message, bool prefer_html_surface);
    void HandleEditorSelectionChanged();
    void CancelFormatPainter(bool preserve_status = false);
    bool EnsurePrintArtifacts(std::filesystem::path* preview_path,
                              std::filesystem::path* printable_path) const;
    ComposeFindTarget ActiveFindTarget() const;
    BTextControl* ControlForFindTarget(ComposeFindTarget target) const;
    BTextControl* ActiveRecipientControl() const;
    bool FocusNextComposeField(bool shift);
    void ApplyInitialFocus();
    bool UsingHtmlSurface() const;

    struct EditorCommandBinding {
        ComposeEditorCommand command = ComposeEditorCommand::kPlain;
        class BMenuItem* item = nullptr;
    };

    HaikuShellHost& shell_host_;
    GuiPreferences gui_preferences_;
    std::string compose_cache_key_;
    class BMenuBar* menu_bar_ = nullptr;
    std::unique_ptr<hermes::RichTextSurface> surface_;
    std::unique_ptr<hermes::HunspellSpellService> spell_service_;
    std::unique_ptr<hermes::TaeMoodWatchAnalyzer> mood_watch_analyzer_;
    std::unique_ptr<hermes::ComposeController> controller_;
    BTextControl* to_control_ = nullptr;
    BTextControl* cc_control_ = nullptr;
    BTextControl* bcc_control_ = nullptr;
    BTextControl* subject_control_ = nullptr;
    BTextControl* reply_to_control_ = nullptr;
    BMenuField* persona_field_ = nullptr;
    BMenuField* surface_field_ = nullptr;
    BMenuField* priority_field_ = nullptr;
    BMenuField* encoding_field_ = nullptr;
    BMenuField* stationery_field_ = nullptr;
    BMenuField* signature_field_ = nullptr;
    class BMenu* insert_recipients_menu_ = nullptr;
    BCheckBox* quoted_printable_box_ = nullptr;
    BCheckBox* text_as_document_box_ = nullptr;
    BCheckBox* word_wrap_box_ = nullptr;
    BCheckBox* tabs_in_body_box_ = nullptr;
    BCheckBox* keep_copies_box_ = nullptr;
    BCheckBox* return_receipt_box_ = nullptr;
    BStringView* banner_view_ = nullptr;
    BStringView* mood_view_ = nullptr;
    BListView* diagnostics_list_ = nullptr;
    BListView* attachment_list_ = nullptr;
    BGroupView* editor_root_container_ = nullptr;
    BView* secondary_headers_container_ = nullptr;
    std::unique_ptr<ComposeEditorHost> editor_host_;
    BTabView* utility_tabs_ = nullptr;
    BView* utility_container_ = nullptr;
    std::unique_ptr<BFilePanel> attachment_open_panel_;
    std::unique_ptr<BFilePanel> picture_open_panel_;
    std::unique_ptr<BMessageRunner> idle_runner_;
    BView* toolbar_view_ = nullptr;
    class BMenuItem* find_again_item_ = nullptr;
    class BMenuItem* show_headers_item_ = nullptr;
    class BMenuItem* insert_picture_item_ = nullptr;
    class BMenuItem* print_item_ = nullptr;
    class BMenuItem* print_one_item_ = nullptr;
    class BMenuItem* print_preview_item_ = nullptr;
    std::vector<EditorCommandBinding> editor_command_bindings_;
    bigtime_t last_edit_time_ = 0;
    std::string transient_status_message_;
    std::string last_find_query_;
    std::size_t last_find_end_offset_ = 0;
    ComposeFindTarget last_find_target_ = ComposeFindTarget::kBody;
    bool headers_visible_ = true;
    bool format_painter_armed_ = false;
    bool ignore_next_format_painter_selection_change_ = false;
    bool applying_format_painter_ = false;
    float remembered_secondary_headers_width_ = -1.0f;
    class BWindow* find_window_ = nullptr;
    BView* find_restore_view_ = nullptr;
    std::optional<ComposeEditorStyleSnapshot> format_painter_snapshot_;
    std::unique_ptr<HaikuToolbarCustomizationWindow> toolbar_customization_window_;
};

}  // namespace hemera::haiku
