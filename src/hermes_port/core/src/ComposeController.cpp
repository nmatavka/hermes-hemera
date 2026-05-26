#include "hermes/ComposeController.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>

#include "hermes/DraftStore.h"
#include "hermes/NicknameStore.h"
#include "hermes/RichTextSurface.h"
#include "hermes/StationeryStore.h"

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

std::string CopySelectedText(const RichTextDocument& document, const TextSelection& selection) {
    if (selection.start > document.plain_text.size()) {
        return {};
    }
    const std::size_t max_length = document.plain_text.size() - selection.start;
    return document.plain_text.substr(selection.start, std::min(selection.length, max_length));
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
    return !document.html_fragment.empty() && !IsWhitespaceOnly(document.html_fragment);
}

}  // namespace

ComposeController::ComposeController(RichTextSurface& surface,
                                     SpellService* spell_service,
                                     MoodWatchAnalyzer* mood_watch_analyzer,
                                     NicknameStore* nickname_store,
                                     StationeryStore* stationery_store)
    : surface_(surface),
      spell_service_(spell_service),
      mood_watch_analyzer_(mood_watch_analyzer),
      nickname_store_(nickname_store),
      stationery_store_(stationery_store) {}

bool ComposeController::Load(const ComposeMessage& message) {
    message_ = message;
    message_.body.read_only = message_.policy.read_only;
    dirty_ = false;
    spell_dirty_ = false;
    mood_dirty_ = false;
    boss_protector_dirty_ = false;
    spell_issues_.clear();
    current_spell_issue_index_ = 0;
    last_mood_watch_result_ = {};
    last_boss_protector_result_ = {};

    if (!surface_.Load(message_.body)) {
        return false;
    }

    if (message_.stationery_name.empty() && !message_.policy.default_stationery_name.empty() &&
        stationery_store_ && Trim(message_.body.plain_text).empty() && Trim(message_.headers.subject).empty()) {
        const bool applied = ApplyStationery(message_.policy.default_stationery_name);
        dirty_ = false;
        return applied;
    }
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

    RichTextDocument body = surface_.Snapshot();
    if (!Trim(stationery->body.plain_text).empty()) {
        if (!Trim(body.plain_text).empty()) {
            body.plain_text += "\n";
        }
        body.plain_text += stationery->body.plain_text;
    }
    if (!Trim(stationery->body.html_fragment).empty()) {
        if (!Trim(body.html_fragment).empty()) {
            body.html_fragment += "\n";
        }
        body.html_fragment += stationery->body.html_fragment;
    }
    body.read_only = message_.policy.read_only;
    message_.stationery_name = stationery->name;
    message_.body = body;

    if (!surface_.Load(body)) {
        return false;
    }
    MarkBodyEdited();
    return true;
}

std::vector<StationeryTemplate> ComposeController::AvailableStationery() const {
    return stationery_store_ ? stationery_store_->Templates() : std::vector<StationeryTemplate>{};
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
    const RichTextDocument snapshot = surface_.Snapshot();
    return surface_.SetSelection({0, snapshot.plain_text.size()});
}

std::string ComposeController::CopySelection() const {
    const RichTextDocument snapshot = surface_.Snapshot();
    return CopySelectedText(snapshot, surface_.Selection());
}

std::string ComposeController::CutSelection() {
    const std::string copied = CopySelection();
    if (!copied.empty()) {
        (void)surface_.ReplaceSelection("");
        MarkBodyEdited();
    }
    return copied;
}

bool ComposeController::PasteText(std::string_view text) {
    if (!surface_.ReplaceSelection(text)) {
        return false;
    }
    MarkBodyEdited();
    return true;
}

bool ComposeController::CheckDocument(const SpellCheckRequest& request) {
    spell_issues_.clear();
    current_spell_issue_index_ = 0;
    if (!spell_service_ || !spell_service_->IsAvailable()) {
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
    return true;
}

void ComposeController::MarkBodyEdited() {
    dirty_ = true;
    spell_dirty_ = true;
    mood_dirty_ = true;
    boss_protector_dirty_ = true;
}

void ComposeController::MarkHeaderEdited(ComposeHeaderField /*field*/) {
    dirty_ = true;
    spell_dirty_ = true;
    mood_dirty_ = true;
    boss_protector_dirty_ = true;
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
        return last_mood_watch_result_;
    }

    const RichTextDocument snapshot = surface_.Snapshot();
    const auto [combined, segments] = BuildMoodWatchText(message_, snapshot);
    if (combined.empty()) {
        last_mood_watch_result_.available = true;
        last_mood_watch_result_.score = 0;
        mood_dirty_ = false;
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

    return validation;
}

bool ComposeController::IsDirty() const {
    return dirty_;
}

}  // namespace hermes
