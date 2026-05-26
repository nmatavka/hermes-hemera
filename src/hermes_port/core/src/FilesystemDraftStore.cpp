#include "hermes/DraftStore.h"

#include <algorithm>
#include <fstream>

#include "hermes/IniSettingsStore.h"

namespace hermes {

namespace {

std::string StyledSendModeToString(const ComposePolicy& policy) {
    if (policy.send_styled_only) {
        return "styled-only";
    }
    if (policy.send_plain_and_styled) {
        return "plain-and-styled";
    }
    return "plain-only";
}

void WritePolicy(IniSettingsStore& metadata, const ComposePolicy& policy) {
    metadata.SetString("Policy", "ReadOnly", policy.read_only ? "1" : "0");
    metadata.SetString("Policy", "AllowStyled", policy.allow_styled ? "1" : "0");
    metadata.SetString("Policy", "SendPlainOnly", policy.send_plain_only ? "1" : "0");
    metadata.SetString("Policy", "WarnOnStyledSend", policy.warn_on_styled_send ? "1" : "0");
    metadata.SetString("Policy", "DefaultSignatureName", policy.default_signature_name);
    metadata.SetString("Policy", "DefaultStationeryName", policy.default_stationery_name);
    metadata.SetString("Policy", "UserSignaturesEnabled", policy.user_signatures_enabled ? "1" : "0");
    metadata.SetString("Policy", "MoodWatchEnabled", policy.mood_watch_enabled ? "1" : "0");
    metadata.SetString("Policy", "MoodCheckBackground", policy.mood_check_background ? "1" : "0");
    metadata.SetString("Policy", "MoodWatchIntervalMs", std::to_string(policy.mood_watch_interval_ms));
    metadata.SetString("Policy",
                       "MoodWarnWhenMightOffend",
                       policy.mood_warn_when_might_offend ? "1" : "0");
    metadata.SetString("Policy",
                       "MoodWarnWhenProbablyOffend",
                       policy.mood_warn_when_probably_offend ? "1" : "0");
    metadata.SetString("Policy", "MoodWarnWhenOnFire", policy.mood_warn_when_on_fire ? "1" : "0");
    metadata.SetString("Policy",
                       "MoodShowCompBadWords",
                       policy.mood_show_comp_bad_words ? "1" : "0");
    metadata.SetString("Policy", "AutoCheckSpelling", policy.auto_check_spelling ? "1" : "0");
    metadata.SetString("Policy", "SpellCheckIntervalMs", std::to_string(policy.spell_check_interval_ms));
    metadata.SetString("Policy",
                       "BossProtectorIntervalMs",
                       std::to_string(policy.boss_protector_interval_ms));
    metadata.SetString("Policy",
                       "BossProtectorWarnOutsideDomains",
                       policy.boss_protector_warn_outside_domains ? "1" : "0");
    metadata.SetString("Policy",
                       "BossProtectorOutsideDomains",
                       policy.boss_protector_outside_domains);
    metadata.SetString("Policy",
                       "BossProtectorWarnInsideDomains",
                       policy.boss_protector_warn_inside_domains ? "1" : "0");
    metadata.SetString("Policy",
                       "BossProtectorInsideDomains",
                       policy.boss_protector_inside_domains);
    metadata.SetString("Policy",
                       "BossProtectorAdditionalWarnDialog",
                       policy.boss_protector_additional_warn_dialog ? "1" : "0");
    metadata.SetString("Policy", "StyledSendMode", StyledSendModeToString(policy));
}

ComposePolicy ReadPolicy(const IniSettingsStore& metadata) {
    ComposePolicy policy;
    policy.read_only = metadata.GetBool("Policy", "ReadOnly", false);
    policy.allow_styled = metadata.GetBool("Policy", "AllowStyled", true);
    policy.send_plain_only = metadata.GetBool("Policy", "SendPlainOnly", false);
    policy.warn_on_styled_send = metadata.GetBool("Policy", "WarnOnStyledSend", false);
    policy.default_signature_name =
        metadata.GetString("Policy", "DefaultSignatureName").value_or("");
    policy.default_stationery_name =
        metadata.GetString("Policy", "DefaultStationeryName").value_or("");
    policy.user_signatures_enabled = metadata.GetBool("Policy", "UserSignaturesEnabled", false);
    policy.mood_watch_enabled = metadata.GetBool("Policy", "MoodWatchEnabled", true);
    policy.mood_check_background = metadata.GetBool("Policy", "MoodCheckBackground", true);
    policy.mood_watch_interval_ms = metadata.GetInt("Policy", "MoodWatchIntervalMs", 1000);
    policy.mood_warn_when_might_offend =
        metadata.GetBool("Policy", "MoodWarnWhenMightOffend", false);
    policy.mood_warn_when_probably_offend =
        metadata.GetBool("Policy", "MoodWarnWhenProbablyOffend", true);
    policy.mood_warn_when_on_fire = metadata.GetBool("Policy", "MoodWarnWhenOnFire", false);
    policy.mood_show_comp_bad_words = metadata.GetBool("Policy", "MoodShowCompBadWords", true);
    policy.auto_check_spelling = metadata.GetBool("Policy", "AutoCheckSpelling", true);
    policy.spell_check_interval_ms = metadata.GetInt("Policy", "SpellCheckIntervalMs", 500);
    policy.boss_protector_interval_ms = metadata.GetInt("Policy", "BossProtectorIntervalMs", 500);
    policy.boss_protector_warn_outside_domains =
        metadata.GetBool("Policy", "BossProtectorWarnOutsideDomains", false);
    policy.boss_protector_outside_domains =
        metadata.GetString("Policy", "BossProtectorOutsideDomains").value_or("");
    policy.boss_protector_warn_inside_domains =
        metadata.GetBool("Policy", "BossProtectorWarnInsideDomains", false);
    policy.boss_protector_inside_domains =
        metadata.GetString("Policy", "BossProtectorInsideDomains").value_or("");
    policy.boss_protector_additional_warn_dialog =
        metadata.GetBool("Policy", "BossProtectorAdditionalWarnDialog", false);

    const std::string mode = metadata.GetString("Policy", "StyledSendMode").value_or("plain-only");
    policy.send_styled_only = mode == "styled-only";
    policy.send_plain_and_styled = mode == "plain-and-styled";
    if (!policy.send_styled_only && !policy.send_plain_and_styled) {
        policy.send_plain_only = true;
    }
    return policy;
}

bool WriteFile(const std::filesystem::path& path,
               std::string_view contents,
               std::string* error_message) {
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        if (error_message) {
            *error_message = "Unable to write " + path.string();
        }
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(output);
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

FilesystemDraftStore::FilesystemDraftStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

bool FilesystemDraftStore::SaveDraft(const ComposeMessage& draft, std::string* error_message) {
    if (draft.id.empty()) {
        if (error_message) {
            *error_message = "Draft id must not be empty.";
        }
        return false;
    }

    std::error_code create_error;
    std::filesystem::create_directories(DraftDirectory(draft.id), create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create draft directory: " + create_error.message();
        }
        return false;
    }

    IniSettingsStore metadata;
    metadata.SetString("Draft", "Id", draft.id);
    metadata.SetString("Draft", "StationeryName", draft.stationery_name);
    metadata.SetString("Draft", "SignatureName", draft.signature_name);
    metadata.SetString("Headers", "To", draft.headers.to);
    metadata.SetString("Headers", "Cc", draft.headers.cc);
    metadata.SetString("Headers", "Bcc", draft.headers.bcc);
    metadata.SetString("Headers", "Subject", draft.headers.subject);
    metadata.SetString("Headers", "FromPersona", draft.headers.from_persona);
    metadata.SetString("Headers", "ReplyTo", draft.headers.reply_to);
    WritePolicy(metadata, draft.policy);

    if (!metadata.SaveToFile(MetadataPath(draft.id), error_message)) {
        return false;
    }
    if (!WriteFile(PlainBodyPath(draft.id), draft.body.plain_text, error_message)) {
        return false;
    }
    if (!WriteFile(HtmlBodyPath(draft.id), draft.body.html_fragment, error_message)) {
        return false;
    }
    return true;
}

std::optional<ComposeMessage> FilesystemDraftStore::GetDraft(std::string_view draft_id) const {
    if (draft_id.empty()) {
        return std::nullopt;
    }

    IniSettingsStore metadata;
    std::string ignored;
    if (!metadata.LoadFromFile(MetadataPath(draft_id), &ignored)) {
        return std::nullopt;
    }

    ComposeMessage draft;
    draft.id = metadata.GetString("Draft", "Id").value_or(std::string(draft_id));
    draft.stationery_name = metadata.GetString("Draft", "StationeryName").value_or("");
    draft.signature_name = metadata.GetString("Draft", "SignatureName").value_or("");
    draft.headers.to = metadata.GetString("Headers", "To").value_or("");
    draft.headers.cc = metadata.GetString("Headers", "Cc").value_or("");
    draft.headers.bcc = metadata.GetString("Headers", "Bcc").value_or("");
    draft.headers.subject = metadata.GetString("Headers", "Subject").value_or("");
    draft.headers.from_persona = metadata.GetString("Headers", "FromPersona").value_or("");
    draft.headers.reply_to = metadata.GetString("Headers", "ReplyTo").value_or("");
    draft.policy = ReadPolicy(metadata);
    draft.body.plain_text = ReadFile(PlainBodyPath(draft_id));
    draft.body.html_fragment = ReadFile(HtmlBodyPath(draft_id));
    draft.body.read_only = draft.policy.read_only;
    return draft;
}

std::vector<ComposeMessage> FilesystemDraftStore::ListDrafts() const {
    std::vector<ComposeMessage> drafts;
    if (!std::filesystem::exists(root_directory_)) {
        return drafts;
    }

    for (const auto& entry : std::filesystem::directory_iterator(root_directory_)) {
        if (!entry.is_directory()) {
            continue;
        }
        auto draft = GetDraft(entry.path().filename().string());
        if (draft) {
            drafts.push_back(std::move(*draft));
        }
    }

    std::sort(drafts.begin(), drafts.end(), [](const ComposeMessage& left, const ComposeMessage& right) {
        return left.id < right.id;
    });
    return drafts;
}

std::filesystem::path FilesystemDraftStore::RootDirectory() const {
    return root_directory_;
}

std::filesystem::path FilesystemDraftStore::DraftDirectory(std::string_view draft_id) const {
    return root_directory_ / std::string(draft_id);
}

std::filesystem::path FilesystemDraftStore::MetadataPath(std::string_view draft_id) const {
    return DraftDirectory(draft_id) / "draft.ini";
}

std::filesystem::path FilesystemDraftStore::PlainBodyPath(std::string_view draft_id) const {
    return DraftDirectory(draft_id) / "body.txt";
}

std::filesystem::path FilesystemDraftStore::HtmlBodyPath(std::string_view draft_id) const {
    return DraftDirectory(draft_id) / "body.html";
}

}  // namespace hermes
