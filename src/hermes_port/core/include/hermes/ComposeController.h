#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/ComposeMessage.h"
#include "hermes/MoodWatchAnalyzer.h"
#include "hermes/SignatureStore.h"
#include "hermes/SpellService.h"
#include "hermes/StationeryStore.h"

namespace hermes {

class DraftStore;
class NicknameStore;
class RichTextSurface;

enum class ComposeHeaderField {
    kTo,
    kCc,
    kBcc,
    kSubject,
    kFromPersona,
    kReplyTo,
};

enum class ComposeTextRegion {
    kSubject,
    kBody,
};

struct ComposeSpellIssue {
    ComposeTextRegion region = ComposeTextRegion::kBody;
    SpellIssue issue;
};

struct ComposeMoodWatchMatch {
    ComposeTextRegion region = ComposeTextRegion::kBody;
    std::size_t offset = 0;
    std::size_t length = 0;
    short collection_id = 0;
    std::string text;
};

struct ComposeMoodWatchResult {
    bool available = false;
    int score = -1;
    std::vector<ComposeMoodWatchMatch> matches;
};

struct BossProtectorHit {
    std::string recipient;
    std::string reason;
};

struct BossProtectorResult {
    bool warning_required = false;
    std::vector<BossProtectorHit> hits;
};

struct StyledSendPlan {
    StyledSendMode mode = StyledSendMode::kPlainTextOnly;
    bool has_styles = false;
    bool should_warn = false;
};

struct AutomaticComposeChecks {
    bool spell_checked = false;
    bool mood_checked = false;
    bool boss_protector_checked = false;
};

struct ComposeSendValidation {
    bool allowed_to_send = true;
    StyledSendPlan styled_send;
    ComposeMoodWatchResult mood_watch;
    BossProtectorResult boss_protector;
    std::vector<std::string> warnings;
    std::vector<std::string> blocking_errors;
};

enum class ComposeDiagnosticSource {
    kSpell,
    kMoodWatch,
    kBossProtector,
    kStyledSend,
};

enum class ComposeDiagnosticSeverity {
    kInfo,
    kWarning,
    kError,
};

struct ComposeVisualDiagnostic {
    ComposeDiagnosticSource source = ComposeDiagnosticSource::kSpell;
    ComposeDiagnosticSeverity severity = ComposeDiagnosticSeverity::kInfo;
    ComposeTextRegion region = ComposeTextRegion::kBody;
    std::size_t offset = 0;
    std::size_t length = 0;
    std::string label;
    std::string message;
};

struct ComposeStatusBanner {
    ComposeDiagnosticSeverity severity = ComposeDiagnosticSeverity::kInfo;
    std::string title;
    std::string message;
};

class ComposeController {
public:
    ComposeController(RichTextSurface& surface,
                      SpellService* spell_service = nullptr,
                      MoodWatchAnalyzer* mood_watch_analyzer = nullptr,
                      NicknameStore* nickname_store = nullptr,
                      StationeryStore* stationery_store = nullptr,
                      SignatureStore* signature_store = nullptr);

    bool Load(const ComposeMessage& message);
    ComposeMessage Snapshot() const;

    bool LoadDraft(const DraftStore& draft_store, std::string_view draft_id);
    bool SaveDraft(DraftStore& draft_store, std::string* error_message = nullptr) const;

    bool UpdateHeader(ComposeHeaderField field, std::string_view value);
    std::string HeaderValue(ComposeHeaderField field) const;

    bool ApplyStationery(std::string_view name);
    std::vector<StationeryTemplate> AvailableStationery() const;
    bool ApplySignature(std::string_view name);
    std::vector<SignatureTemplate> AvailableSignatures() const;
    const std::vector<ComposeAttachment>& Attachments() const;
    bool AddAttachment(const ComposeAttachment& attachment, std::string* error_message = nullptr);
    bool RemoveAttachment(std::size_t index);

    bool Undo();
    bool Redo();
    bool SelectAll();
    std::string CopySelection() const;
    std::string CutSelection();
    bool PasteText(std::string_view text);

    bool CheckDocument(const SpellCheckRequest& request = {});
    const std::vector<ComposeSpellIssue>& SpellIssues() const;
    std::vector<std::string> SuggestionsForCurrentIssue(const SpellCheckRequest& request = {}) const;
    bool IgnoreCurrentWord();
    bool AddCurrentWord(const SpellCheckRequest& request = {});
    bool ReplaceCurrent(std::string_view replacement);

    void MarkBodyEdited();
    void MarkHeaderEdited(ComposeHeaderField field);
    AutomaticComposeChecks ServiceAutomaticChecks(std::chrono::milliseconds idle_time);

    ComposeMoodWatchResult RunMoodWatch();
    BossProtectorResult RunBossProtector();
    StyledSendPlan ResolveStyledSendPlan() const;
    ComposeSendValidation ValidateForSend();
    const std::vector<ComposeVisualDiagnostic>& Diagnostics() const;
    std::optional<ComposeStatusBanner> StatusBanner() const;

    bool IsDirty() const;

private:
    void RefreshVisualFeedback();
    void RefreshBanner();
    void RefreshBodyDiagnostics();
    void MaybeDetachManagedSignature();
    bool RemoveManagedSignatureFromBody();
    bool InsertSignatureIntoBody(const SignatureTemplate& signature);

    RichTextSurface& surface_;
    SpellService* spell_service_ = nullptr;
    MoodWatchAnalyzer* mood_watch_analyzer_ = nullptr;
    NicknameStore* nickname_store_ = nullptr;
    StationeryStore* stationery_store_ = nullptr;
    SignatureStore* signature_store_ = nullptr;

    ComposeMessage message_;
    bool dirty_ = false;
    bool spell_dirty_ = false;
    bool mood_dirty_ = false;
    bool boss_protector_dirty_ = false;
    std::vector<ComposeSpellIssue> spell_issues_;
    std::size_t current_spell_issue_index_ = 0;
    ComposeMoodWatchResult last_mood_watch_result_;
    BossProtectorResult last_boss_protector_result_;
    std::vector<ComposeVisualDiagnostic> diagnostics_;
    std::optional<ComposeStatusBanner> status_banner_;
    std::optional<ComposeSendValidation> last_send_validation_;
};

}  // namespace hermes
