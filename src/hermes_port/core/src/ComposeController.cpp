#include "hermes/ComposeController.h"
#include "hermes/RichTextFormat.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <utility>

#include "hermes/DraftStore.h"
#include "hermes/NicknameStore.h"
#include "hermes/RichTextSurface.h"

namespace hermes {

namespace {

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool IsWhitespaceOnly(std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
}

std::string ToLower(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (char ch : value) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return lowered;
}

std::string HeaderFieldValue(const ComposeHeaders& headers, ComposeHeaderField field) {
    switch (field) {
        case ComposeHeaderField::kTo:
            return headers.to;
        case ComposeHeaderField::kCc:
            return headers.cc;
        case ComposeHeaderField::kBcc:
            return headers.bcc;
        case ComposeHeaderField::kSubject:
            return headers.subject;
        case ComposeHeaderField::kFromPersona:
            return headers.from_persona;
        case ComposeHeaderField::kReplyTo:
            return headers.reply_to;
    }
    return {};
}

void SetHeaderFieldValue(ComposeHeaders& headers, ComposeHeaderField field, std::string value) {
    switch (field) {
        case ComposeHeaderField::kTo:
            headers.to = std::move(value);
            return;
        case ComposeHeaderField::kCc:
            headers.cc = std::move(value);
            return;
        case ComposeHeaderField::kBcc:
            headers.bcc = std::move(value);
            return;
        case ComposeHeaderField::kSubject:
            headers.subject = std::move(value);
            return;
        case ComposeHeaderField::kFromPersona:
            headers.from_persona = std::move(value);
            return;
        case ComposeHeaderField::kReplyTo:
            headers.reply_to = std::move(value);
            return;
    }
}

std::string JoinHeaderValue(std::string base, std::string_view addition) {
    const std::string trimmed_addition = Trim(std::string(addition));
    if (trimmed_addition.empty()) {
        return base;
    }
    if (Trim(base).empty()) {
        return trimmed_addition;
    }
    if (base.find(trimmed_addition) != std::string::npos) {
        return base;
    }
    base += ", ";
    base += trimmed_addition;
    return base;
}

std::vector<std::string> SplitCsvLike(std::string_view value) {
    std::vector<std::string> result;
    std::string current;
    for (char ch : value) {
        if (ch == ',' || ch == ';' || ch == '\n' || ch == '\r') {
            const std::string trimmed = Trim(current);
            if (!trimmed.empty()) {
                result.push_back(trimmed);
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }

    const std::string trimmed = Trim(current);
    if (!trimmed.empty()) {
        result.push_back(trimmed);
    }
    return result;
}

std::string ExtractEmailAddress(std::string token) {
    token = Trim(std::move(token));
    const std::size_t open = token.find('<');
    const std::size_t close = token.find('>');
    if (open != std::string::npos && close != std::string::npos && close > open) {
        return Trim(token.substr(open + 1, close - open - 1));
    }
    return token;
}

bool DomainMatches(std::string_view configured_domain, std::string_view recipient_domain) {
    std::string configured = ToLower(Trim(std::string(configured_domain)));
    std::string recipient = ToLower(Trim(std::string(recipient_domain)));
    if (configured.rfind("*.", 0) == 0) {
        configured.erase(0, 2);
    }
    if (configured.empty() || recipient.empty()) {
        return false;
    }

    if (configured.size() >= recipient.size()) {
        return configured.compare(configured.size() - recipient.size(), recipient.size(), recipient) == 0;
    }
    return recipient.compare(recipient.size() - configured.size(), configured.size(), configured) == 0;
}

bool BossProtectorDomainMatch(const ComposePolicy& policy, std::string_view recipient) {
    const std::size_t at = recipient.find('@');
    if (at == std::string::npos || at + 1 >= recipient.size()) {
        return false;
    }

    const std::string recipient_domain = std::string(recipient.substr(at + 1));
    if (policy.boss_protector_warn_inside_domains) {
        for (const auto& candidate : SplitCsvLike(policy.boss_protector_inside_domains)) {
            if (DomainMatches(candidate, recipient_domain)) {
                return true;
            }
        }
        return false;
    }

    if (policy.boss_protector_warn_outside_domains) {
        const auto domains = SplitCsvLike(policy.boss_protector_outside_domains);
        if (domains.empty()) {
            return false;
        }

        for (const auto& candidate : domains) {
            if (DomainMatches(candidate, recipient_domain)) {
                return false;
            }
        }
        return true;
    }

    return false;
}

std::vector<std::string> ExpandRecipients(const NicknameStore* nickname_store, std::string_view value) {
    std::vector<std::string> result;
    for (const auto& token : SplitCsvLike(value)) {
        if (nickname_store) {
            if (const auto nickname = nickname_store->FindNickname(token)) {
                for (const auto& address : nickname->addresses) {
                    const std::string email = ExtractEmailAddress(address);
                    if (!email.empty()) {
                        result.push_back(email);
                    }
                }
                continue;
            }
        }

        const std::string email = ExtractEmailAddress(token);
        if (!email.empty()) {
            result.push_back(email);
        }
    }
    return result;
}

struct Segment {
    ComposeTextRegion region = ComposeTextRegion::kBody;
    std::size_t start = 0;
    std::size_t length = 0;
    std::string text;
};

std::pair<std::string, std::vector<Segment>> BuildMoodWatchText(const ComposeMessage& message,
                                                                const RichTextDocument& body_snapshot) {
    std::string combined;
    std::vector<Segment> segments;

    const std::array<std::pair<ComposeTextRegion, std::string>, 2> fields = {{
        {ComposeTextRegion::kSubject, message.headers.subject},
        {ComposeTextRegion::kBody, body_snapshot.plain_text},
    }};

    for (const auto& field : fields) {
        if (Trim(field.second).empty()) {
            continue;
        }
        if (!combined.empty()) {
            combined.push_back('\n');
        }
        const std::size_t start = combined.size();
        combined += field.second;
        segments.push_back({field.first, start, field.second.size(), field.second});
    }

    return {combined, segments};
}

ComposeTextRegion RegionForOffset(const std::vector<Segment>& segments,
                                  std::size_t offset,
                                  std::size_t* local_offset) {
    for (const auto& segment : segments) {
        if (offset >= segment.start && offset < (segment.start + segment.length)) {
            if (local_offset) {
                *local_offset = offset - segment.start;
            }
            return segment.region;
        }
    }
    if (local_offset) {
        *local_offset = 0;
    }
    return ComposeTextRegion::kBody;
}

std::string SegmentTextAt(const std::vector<Segment>& segments,
                          ComposeTextRegion region,
                          std::size_t offset,
                          std::size_t length) {
    for (const auto& segment : segments) {
        if (segment.region != region) {
            continue;
        }
        if (offset <= segment.text.size()) {
            return segment.text.substr(offset, std::min(length, segment.text.size() - offset));
        }
    }
    return {};
}

bool HasStyles(const RichTextDocument& document) {
    return HasAuthenticStyledContent(document);
}

bool IsDefaultOptions(const ComposeOptions& options) {
    return options.priority == ComposePriority::kNormal &&
           options.attachment_encoding == AttachmentEncodingMode::kMime &&
           options.keep_copies == false &&
           options.request_read_receipt == false &&
           options.quoted_printable == true &&
           options.word_wrap == true &&
           options.tabs_in_body == true &&
           options.text_as_document == false;
}

TextDiagnosticSeverity ToTextSeverity(ComposeDiagnosticSeverity severity) {
    switch (severity) {
        case ComposeDiagnosticSeverity::kInfo:
            return TextDiagnosticSeverity::kInfo;
        case ComposeDiagnosticSeverity::kWarning:
            return TextDiagnosticSeverity::kWarning;
        case ComposeDiagnosticSeverity::kError:
            return TextDiagnosticSeverity::kError;
    }
    return TextDiagnosticSeverity::kInfo;
}

std::optional<ComposeStatusBanner> BannerFromValidation(const ComposeSendValidation& validation) {
    if (!validation.blocking_errors.empty()) {
        return ComposeStatusBanner{
            ComposeDiagnosticSeverity::kError,
            "Send blocked",
            validation.blocking_errors.front(),
        };
    }
    if (!validation.warnings.empty()) {
        return ComposeStatusBanner{
            ComposeDiagnosticSeverity::kWarning,
            "Send confirmation required",
            validation.warnings.front(),
        };
    }
    return std::nullopt;
}

}  // namespace

ComposeController::ComposeController(RichTextSurface& surface,
                                     SpellService* spell_service,
                                     MoodWatchAnalyzer* mood_watch_analyzer,
                                     NicknameStore* nickname_store,
                                     StationeryStore* stationery_store,
                                     SignatureStore* signature_store)
    : surface_(surface),
      spell_service_(spell_service),
      mood_watch_analyzer_(mood_watch_analyzer),
      nickname_store_(nickname_store),
      stationery_store_(stationery_store),
      signature_store_(signature_store) {}

bool ComposeController::Load(const ComposeMessage& message) {
    message_ = message;
    message_.body.read_only = message_.policy.read_only;
    if (IsDefaultOptions(message_.options)) {
        message_.options = message_.policy.default_options;
    }
    dirty_ = false;
    spell_dirty_ = true;
    mood_dirty_ = true;
    boss_protector_dirty_ = true;
    spell_issues_.clear();
    current_spell_issue_index_ = 0;
    last_mood_watch_result_ = {};
    last_boss_protector_result_ = {};
    last_send_validation_.reset();
    diagnostics_.clear();
    status_banner_.reset();

    if (!surface_.Load(message_.body)) {
        return false;
    }

    bool auto_applied = false;
    if (message_.stationery_name.empty() && !message_.policy.default_stationery_name.empty() &&
        stationery_store_ && Trim(message_.body.plain_text).empty() && Trim(message_.headers.subject).empty()) {
        if (!ApplyStationery(message_.policy.default_stationery_name)) {
            return false;
        }
        auto_applied = true;
    }

    if (message_.signature_name.empty() && !message_.policy.default_signature_name.empty()) {
        message_.signature_name = message_.policy.default_signature_name;
    }

    if (!message_.managed_signature.attached && !message_.signature_name.empty() && signature_store_) {
        if (!ApplySignature(message_.signature_name)) {
            return false;
        }
        auto_applied = true;
    }

    if (auto_applied) {
        dirty_ = false;
    }

    RefreshVisualFeedback();
    RefreshBanner();
    return true;
}

ComposeMessage ComposeController::Snapshot() const {
    ComposeMessage snapshot = message_;
    snapshot.body = surface_.Snapshot();
    snapshot.body.read_only = message_.policy.read_only;
    return snapshot;
}

bool ComposeController::LoadDraft(const DraftStore& draft_store, std::string_view draft_id) {
    const auto draft = draft_store.GetDraft(draft_id);
    return draft ? Load(*draft) : false;
}

bool ComposeController::SaveDraft(DraftStore& draft_store, std::string* error_message) const {
    return draft_store.SaveDraft(Snapshot(), error_message);
}

bool ComposeController::UpdateHeader(ComposeHeaderField field, std::string_view value) {
    SetHeaderFieldValue(message_.headers, field, std::string(value));
    MarkHeaderEdited(field);
    return true;
}

std::string ComposeController::HeaderValue(ComposeHeaderField field) const {
    return HeaderFieldValue(message_.headers, field);
}

bool ComposeController::ApplyStationery(std::string_view name) {
    if (!stationery_store_) {
        return false;
    }

    const auto stationery = stationery_store_->Find(name);
    if (!stationery) {
        return false;
    }

    message_.headers.to = JoinHeaderValue(message_.headers.to, stationery->headers.to);
    message_.headers.cc = JoinHeaderValue(message_.headers.cc, stationery->headers.cc);
    message_.headers.bcc = JoinHeaderValue(message_.headers.bcc, stationery->headers.bcc);

    if (!Trim(stationery->headers.subject).empty()) {
        if (Trim(message_.headers.subject).empty()) {
            message_.headers.subject = stationery->headers.subject;
        } else if (message_.headers.subject.find(stationery->headers.subject) == std::string::npos) {
            message_.headers.subject = stationery->headers.subject + ": " + message_.headers.subject;
        }
    }

    if (message_.headers.from_persona.empty() && !stationery->persona.empty()) {
        message_.headers.from_persona = stationery->persona;
    }
    if (message_.signature_name.empty() && !stationery->signature_name.empty()) {
        message_.signature_name = stationery->signature_name;
    }

    RichTextDocument body = MergeRichTextDocuments(surface_.Snapshot(), stationery->body, "\n");
    body.read_only = message_.policy.read_only;
    message_.stationery_name = stationery->name;
    message_.body = body;

    if (!surface_.Load(body)) {
        return false;
    }

    if (!message_.signature_name.empty() && !message_.managed_signature.attached && signature_store_) {
        if (!ApplySignature(message_.signature_name)) {
            return false;
        }
    } else {
        MarkBodyEdited();
    }
    return true;
}

std::vector<StationeryTemplate> ComposeController::AvailableStationery() const {
    return stationery_store_ ? stationery_store_->Templates() : std::vector<StationeryTemplate>{};
}

bool ComposeController::ApplySignature(std::string_view name) {
    if (!signature_store_) {
        return false;
    }

    const auto signature = signature_store_->Find(name);
    if (!signature) {
        return false;
    }

    if (!RemoveManagedSignatureFromBody()) {
        return false;
    }

    if (!InsertSignatureIntoBody(*signature)) {
        return false;
    }

    message_.signature_name = signature->name;
    MarkBodyEdited();
    return true;
}

std::vector<SignatureTemplate> ComposeController::AvailableSignatures() const {
    return signature_store_ ? signature_store_->Templates() : std::vector<SignatureTemplate>{};
}

const ComposeOptions& ComposeController::Options() const {
    return message_.options;
}

bool ComposeController::UpdateOptions(const ComposeOptions& options) {
    message_.options = options;
    dirty_ = true;
    last_send_validation_.reset();
    RefreshBanner();
    return true;
}

const std::vector<ComposeAttachment>& ComposeController::Attachments() const {
    return message_.attachments;
}

bool ComposeController::AddAttachment(const ComposeAttachment& attachment, std::string* error_message) {
    ComposeAttachment stored = attachment;
    if (stored.display_name.empty() && !stored.source_path.empty()) {
        stored.display_name = stored.source_path.filename().string();
    }
    if (stored.display_name.empty()) {
        if (error_message) {
            *error_message = "Attachment display name must not be empty.";
        }
        return false;
    }
    if (stored.source_path.empty()) {
        if (error_message) {
            *error_message = "Attachment source path must not be empty.";
        }
        return false;
    }

    std::error_code stat_error;
    if (!std::filesystem::exists(stored.source_path, stat_error) || stat_error) {
        if (error_message) {
            *error_message = "Attachment source file is unavailable: " + stored.source_path.string();
        }
        return false;
    }
    if (stored.size == 0) {
        const auto size = std::filesystem::file_size(stored.source_path, stat_error);
        if (!stat_error) {
            stored.size = static_cast<std::uint64_t>(size);
        }
    }

    message_.attachments.push_back(std::move(stored));
    dirty_ = true;
    return true;
}

bool ComposeController::RemoveAttachment(std::size_t index) {
    if (index >= message_.attachments.size()) {
        return false;
    }
    message_.attachments.erase(message_.attachments.begin() + static_cast<std::ptrdiff_t>(index));
    dirty_ = true;
    return true;
}

bool ComposeController::Undo() {
    if (!surface_.Undo()) {
        return false;
    }
    MarkBodyEdited();
    return true;
}

bool ComposeController::Redo() {
    if (!surface_.Redo()) {
        return false;
    }
    MarkBodyEdited();
    return true;
}

bool ComposeController::SelectAll() {
    return surface_.SelectAll();
}

std::string ComposeController::CopySelection() const {
    return surface_.CopySelection();
}

std::string ComposeController::CutSelection() {
    const std::string copied = surface_.CutSelection();
    if (!copied.empty()) {
        MarkBodyEdited();
    }
    return copied;
}

bool ComposeController::PasteText(std::string_view text) {
    if (!surface_.Paste(text)) {
        return false;
    }
    MarkBodyEdited();
    return true;
}

bool ComposeController::CheckDocument(const SpellCheckRequest& request) {
    spell_issues_.clear();
    current_spell_issue_index_ = 0;
    if (!spell_service_ || !spell_service_->IsAvailable()) {
        RefreshVisualFeedback();
        RefreshBanner();
        return false;
    }

    for (const auto& issue : spell_service_->Check(message_.headers.subject, request)) {
        spell_issues_.push_back({ComposeTextRegion::kSubject, issue});
    }

    const RichTextDocument snapshot = surface_.Snapshot();
    for (const auto& issue : spell_service_->Check(snapshot.plain_text, request)) {
        spell_issues_.push_back({ComposeTextRegion::kBody, issue});
    }

    spell_dirty_ = false;
    RefreshVisualFeedback();
    RefreshBanner();
    return true;
}

const std::vector<ComposeSpellIssue>& ComposeController::SpellIssues() const {
    return spell_issues_;
}

std::vector<std::string> ComposeController::SuggestionsForCurrentIssue(const SpellCheckRequest& request) const {
    if (!spell_service_ || !spell_service_->IsAvailable() || current_spell_issue_index_ >= spell_issues_.size()) {
        return {};
    }
    return spell_service_->Suggest(spell_issues_[current_spell_issue_index_].issue.word, request);
}

bool ComposeController::IgnoreCurrentWord() {
    if (!spell_service_ || current_spell_issue_index_ >= spell_issues_.size()) {
        return false;
    }
    spell_service_->IgnoreWord(spell_issues_[current_spell_issue_index_].issue.word);
    ++current_spell_issue_index_;
    RefreshVisualFeedback();
    RefreshBanner();
    return true;
}

bool ComposeController::AddCurrentWord(const SpellCheckRequest& request) {
    if (!spell_service_ || current_spell_issue_index_ >= spell_issues_.size()) {
        return false;
    }
    const bool added =
        spell_service_->AddWordToUserDictionary(spell_issues_[current_spell_issue_index_].issue.word, request);
    if (added) {
        ++current_spell_issue_index_;
        RefreshVisualFeedback();
        RefreshBanner();
    }
    return added;
}

bool ComposeController::ReplaceCurrent(std::string_view replacement) {
    if (current_spell_issue_index_ >= spell_issues_.size()) {
        return false;
    }

    const ComposeSpellIssue& issue = spell_issues_[current_spell_issue_index_];
    if (issue.region == ComposeTextRegion::kSubject) {
        if (issue.issue.offset > message_.headers.subject.size()) {
            return false;
        }
        message_.headers.subject.replace(issue.issue.offset, issue.issue.length, replacement);
        MarkHeaderEdited(ComposeHeaderField::kSubject);
    } else {
        if (!surface_.SetSelection({issue.issue.offset, issue.issue.length})) {
            return false;
        }
        if (!surface_.ReplaceSelection(replacement)) {
            return false;
        }
        MarkBodyEdited();
    }

    ++current_spell_issue_index_;
    RefreshVisualFeedback();
    RefreshBanner();
    return true;
}

void ComposeController::MarkBodyEdited() {
    MaybeDetachManagedSignature();
    dirty_ = true;
    spell_dirty_ = true;
    mood_dirty_ = true;
    boss_protector_dirty_ = true;
    spell_issues_.clear();
    current_spell_issue_index_ = 0;
    last_mood_watch_result_ = {};
    last_boss_protector_result_ = {};
    last_send_validation_.reset();
    RefreshVisualFeedback();
    RefreshBanner();
}

void ComposeController::MarkHeaderEdited(ComposeHeaderField /*field*/) {
    dirty_ = true;
    spell_dirty_ = true;
    mood_dirty_ = true;
    boss_protector_dirty_ = true;
    spell_issues_.clear();
    current_spell_issue_index_ = 0;
    last_mood_watch_result_ = {};
    last_boss_protector_result_ = {};
    last_send_validation_.reset();
    RefreshVisualFeedback();
    RefreshBanner();
}

AutomaticComposeChecks ComposeController::ServiceAutomaticChecks(std::chrono::milliseconds idle_time) {
    AutomaticComposeChecks checks;

    if (spell_dirty_ && message_.policy.auto_check_spelling &&
        idle_time >= std::chrono::milliseconds(message_.policy.spell_check_interval_ms)) {
        checks.spell_checked = CheckDocument();
    }

    if (mood_dirty_ && message_.policy.mood_watch_enabled && message_.policy.mood_check_background &&
        idle_time >= std::chrono::milliseconds(message_.policy.mood_watch_interval_ms)) {
        RunMoodWatch();
        checks.mood_checked = true;
    }

    if (boss_protector_dirty_ &&
        idle_time >= std::chrono::milliseconds(message_.policy.boss_protector_interval_ms)) {
        RunBossProtector();
        checks.boss_protector_checked = true;
    }

    return checks;
}

ComposeMoodWatchResult ComposeController::RunMoodWatch() {
    last_mood_watch_result_ = {};
    if (!mood_watch_analyzer_ || !message_.policy.mood_watch_enabled || !mood_watch_analyzer_->IsAvailable()) {
        RefreshVisualFeedback();
        RefreshBanner();
        return last_mood_watch_result_;
    }

    const RichTextDocument snapshot = surface_.Snapshot();
    const auto [combined, segments] = BuildMoodWatchText(message_, snapshot);
    if (combined.empty()) {
        last_mood_watch_result_.available = true;
        last_mood_watch_result_.score = 0;
        mood_dirty_ = false;
        RefreshVisualFeedback();
        RefreshBanner();
        return last_mood_watch_result_;
    }

    MoodWatchOptions options;
    options.contains_html = HasStyles(snapshot);
    options.ignore_safe_text = true;

    const MoodWatchResult result = mood_watch_analyzer_->Analyze(combined, options);
    last_mood_watch_result_.available = result.available;
    last_mood_watch_result_.score = result.score;
    for (const auto& match : result.matches) {
        std::size_t local_offset = 0;
        const ComposeTextRegion region = RegionForOffset(segments, match.offset, &local_offset);
        last_mood_watch_result_.matches.push_back({
            region,
            local_offset,
            match.length,
            match.collection_id,
            SegmentTextAt(segments, region, local_offset, match.length),
        });
    }

    mood_dirty_ = false;
    RefreshVisualFeedback();
    RefreshBanner();
    return last_mood_watch_result_;
}

BossProtectorResult ComposeController::RunBossProtector() {
    last_boss_protector_result_ = {};

    std::vector<std::string> recipients;
    const auto to = ExpandRecipients(nickname_store_, message_.headers.to);
    const auto cc = ExpandRecipients(nickname_store_, message_.headers.cc);
    const auto bcc = ExpandRecipients(nickname_store_, message_.headers.bcc);
    recipients.insert(recipients.end(), to.begin(), to.end());
    recipients.insert(recipients.end(), cc.begin(), cc.end());
    recipients.insert(recipients.end(), bcc.begin(), bcc.end());

    std::vector<std::string> bp_addresses;
    if (nickname_store_) {
        for (const auto& entry : nickname_store_->Entries()) {
            if (!entry.bp_list) {
                continue;
            }
            for (const auto& address : entry.addresses) {
                bp_addresses.push_back(ToLower(ExtractEmailAddress(address)));
            }
        }
    }

    for (const auto& recipient : recipients) {
        const std::string normalized = ToLower(recipient);
        if (std::find(bp_addresses.begin(), bp_addresses.end(), normalized) != bp_addresses.end()) {
            last_boss_protector_result_.hits.push_back({recipient, "Recipient is on the Boss Protector list."});
            continue;
        }
        if (BossProtectorDomainMatch(message_.policy, recipient)) {
            last_boss_protector_result_.hits.push_back(
                {recipient, "Recipient domain matched the Boss Protector rule set."});
        }
    }

    last_boss_protector_result_.warning_required = !last_boss_protector_result_.hits.empty();
    boss_protector_dirty_ = false;
    RefreshVisualFeedback();
    RefreshBanner();
    return last_boss_protector_result_;
}

StyledSendPlan ComposeController::ResolveStyledSendPlan() const {
    const RichTextDocument snapshot = surface_.Snapshot();
    const bool has_styles = HasStyles(snapshot);
    return {
        ResolveStyledSendMode(message_.policy, has_styles),
        has_styles,
        has_styles && message_.policy.warn_on_styled_send,
    };
}

ComposeSendValidation ComposeController::ValidateForSend() {
    ComposeSendValidation validation;
    validation.allowed_to_send = !message_.policy.read_only;
    validation.styled_send = ResolveStyledSendPlan();
    validation.mood_watch = RunMoodWatch();
    validation.boss_protector = RunBossProtector();

    if (!validation.allowed_to_send) {
        validation.blocking_errors.push_back("Read-only compose messages cannot be sent.");
    }

    if (validation.styled_send.should_warn) {
        validation.warnings.push_back("Styled content is present and send-time styled/plain confirmation is required.");
    }

    if (validation.mood_watch.available) {
        if (validation.mood_watch.score >= 4 && message_.policy.mood_warn_when_on_fire) {
            validation.warnings.push_back("MoodWatch flagged this message at the highest level.");
        } else if (validation.mood_watch.score >= 3 && message_.policy.mood_warn_when_probably_offend) {
            validation.warnings.push_back("MoodWatch thinks this message is probably offensive.");
        } else if (validation.mood_watch.score >= 2 && message_.policy.mood_warn_when_might_offend) {
            validation.warnings.push_back("MoodWatch thinks this message might offend the recipient.");
        }
    }

    if (validation.boss_protector.warning_required) {
        validation.warnings.push_back("Boss Protector requires confirmation for one or more recipients.");
    }

    last_send_validation_ = validation;
    RefreshVisualFeedback();
    RefreshBanner();
    return validation;
}

const std::vector<ComposeVisualDiagnostic>& ComposeController::Diagnostics() const {
    return diagnostics_;
}

std::optional<ComposeStatusBanner> ComposeController::StatusBanner() const {
    return status_banner_;
}

bool ComposeController::IsDirty() const {
    return dirty_;
}

void ComposeController::RefreshVisualFeedback() {
    diagnostics_.clear();

    for (const auto& issue : spell_issues_) {
        diagnostics_.push_back({
            ComposeDiagnosticSource::kSpell,
            ComposeDiagnosticSeverity::kWarning,
            issue.region,
            issue.issue.offset,
            issue.issue.length,
            "Spelling",
            "Possible misspelling: " + issue.issue.word,
        });
    }

    for (const auto& match : last_mood_watch_result_.matches) {
        diagnostics_.push_back({
            ComposeDiagnosticSource::kMoodWatch,
            ComposeDiagnosticSeverity::kWarning,
            match.region,
            match.offset,
            match.length,
            "MoodWatch",
            match.text.empty() ? "MoodWatch match" : "MoodWatch flagged: " + match.text,
        });
    }

    for (const auto& hit : last_boss_protector_result_.hits) {
        diagnostics_.push_back({
            ComposeDiagnosticSource::kBossProtector,
            ComposeDiagnosticSeverity::kWarning,
            ComposeTextRegion::kSubject,
            0,
            0,
            "Boss Protector",
            hit.recipient + ": " + hit.reason,
        });
    }

    const StyledSendPlan styled_send = last_send_validation_ ? last_send_validation_->styled_send
                                                             : ResolveStyledSendPlan();
    if (styled_send.should_warn) {
        diagnostics_.push_back({
            ComposeDiagnosticSource::kStyledSend,
            ComposeDiagnosticSeverity::kInfo,
            ComposeTextRegion::kBody,
            0,
            0,
            "Styled content",
            "Styled content will require send-time confirmation.",
        });
    }

    RefreshBodyDiagnostics();
}

void ComposeController::RefreshBanner() {
    status_banner_.reset();

    if (last_send_validation_) {
        status_banner_ = BannerFromValidation(*last_send_validation_);
        if (status_banner_) {
            return;
        }
    }

    if (last_boss_protector_result_.warning_required) {
        status_banner_ = ComposeStatusBanner{
            ComposeDiagnosticSeverity::kWarning,
            "Boss Protector",
            "One or more recipients require extra confirmation.",
        };
        return;
    }

    if (last_mood_watch_result_.available) {
        if (last_mood_watch_result_.score >= 4 && message_.policy.mood_warn_when_on_fire) {
            status_banner_ = ComposeStatusBanner{
                ComposeDiagnosticSeverity::kWarning,
                "MoodWatch",
                "MoodWatch flagged this message at the highest level.",
            };
            return;
        }
        if (last_mood_watch_result_.score >= 3 && message_.policy.mood_warn_when_probably_offend) {
            status_banner_ = ComposeStatusBanner{
                ComposeDiagnosticSeverity::kWarning,
                "MoodWatch",
                "MoodWatch thinks this message is probably offensive.",
            };
            return;
        }
        if (last_mood_watch_result_.score >= 2 && message_.policy.mood_warn_when_might_offend) {
            status_banner_ = ComposeStatusBanner{
                ComposeDiagnosticSeverity::kWarning,
                "MoodWatch",
                "MoodWatch thinks this message might offend the recipient.",
            };
            return;
        }
    }

    if (!spell_issues_.empty()) {
        status_banner_ = ComposeStatusBanner{
            ComposeDiagnosticSeverity::kInfo,
            "Spelling",
            "Spelling found " + std::to_string(spell_issues_.size()) + " issue(s).",
        };
    }
}

void ComposeController::RefreshBodyDiagnostics() {
    std::vector<TextDiagnostic> body_diagnostics;
    for (const auto& diagnostic : diagnostics_) {
        if (diagnostic.region != ComposeTextRegion::kBody || diagnostic.length == 0) {
            continue;
        }

        TextDiagnosticKind kind = TextDiagnosticKind::kSpell;
        switch (diagnostic.source) {
            case ComposeDiagnosticSource::kSpell:
                kind = TextDiagnosticKind::kSpell;
                break;
            case ComposeDiagnosticSource::kMoodWatch:
                kind = TextDiagnosticKind::kMoodWatch;
                break;
            case ComposeDiagnosticSource::kBossProtector:
            case ComposeDiagnosticSource::kStyledSend:
                kind = TextDiagnosticKind::kStyledContent;
                break;
        }

        body_diagnostics.push_back({
            kind,
            ToTextSeverity(diagnostic.severity),
            diagnostic.offset,
            diagnostic.length,
            diagnostic.message,
        });
    }

    if (body_diagnostics.empty()) {
        surface_.ClearDiagnostics();
    } else {
        surface_.SetDiagnostics(std::move(body_diagnostics));
    }
}

void ComposeController::MaybeDetachManagedSignature() {
    if (!message_.managed_signature.attached) {
        return;
    }

    const RichTextDocument snapshot = surface_.Snapshot();
    if (message_.managed_signature.start > snapshot.plain_text.size()) {
        message_.managed_signature.attached = false;
        return;
    }

    const std::size_t available = snapshot.plain_text.size() - message_.managed_signature.start;
    if (message_.managed_signature.length > available) {
        message_.managed_signature.attached = false;
        return;
    }

    const std::string current =
        snapshot.plain_text.substr(message_.managed_signature.start, message_.managed_signature.length);
    if (current != message_.managed_signature.plain_text) {
        message_.managed_signature.attached = false;
    }
}

bool ComposeController::RemoveManagedSignatureFromBody() {
    if (!message_.managed_signature.attached) {
        return true;
    }

    MaybeDetachManagedSignature();
    if (!message_.managed_signature.attached) {
        return true;
    }

    if (!surface_.SetSelection({message_.managed_signature.start, message_.managed_signature.length})) {
        return false;
    }
    if (!surface_.ReplaceSelection("")) {
        return false;
    }

    message_.managed_signature = {};
    return true;
}

bool ComposeController::InsertSignatureIntoBody(const SignatureTemplate& signature) {
    const RichTextDocument before = surface_.Snapshot();
    if (before.read_only) {
        return false;
    }

    std::string prefix;
    if (!before.plain_text.empty()) {
        prefix = before.plain_text.back() == '\n' ? "\n" : "\n\n";
    }

    RichTextDocument inserted_signature = signature.body;
    if (!prefix.empty()) {
        RichTextDocument prefix_document;
        prefix_document.plain_text = prefix;
        inserted_signature = MergeRichTextDocuments(prefix_document, inserted_signature, "");
    }
    RichTextDocument merged = MergeRichTextDocuments(before, inserted_signature, "");
    if (!surface_.Load(merged)) {
        return false;
    }

    const RichTextDocument snapshot = surface_.Snapshot();

    message_.managed_signature.attached = true;
    message_.managed_signature.name = signature.name;
    message_.managed_signature.start = before.plain_text.size() + prefix.size();
    message_.managed_signature.length = signature.body.plain_text.size();
    message_.managed_signature.plain_text = signature.body.plain_text;
    return true;
}

}  // namespace hermes
