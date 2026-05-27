#include "hermes/ComposeMessage.h"

#include <algorithm>
#include <cctype>

namespace hermes {

namespace {

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string NormalizeLegacyDefaultValue(std::string value) {
    value = Trim(std::move(value));
    std::string lowered;
    lowered.reserve(value.size());
    for (char ch : value) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (lowered == "<no default>" || lowered == "no default") {
        return {};
    }
    return value;
}

AttachmentEncodingMode AttachmentEncodingFromSettings(const SettingsStore& settings,
                                                      std::string_view section) {
    if (settings.GetBool(section, "SendBinHex", false)) {
        return AttachmentEncodingMode::kBinHex;
    }
    if (settings.GetBool(section, "SendUuencode", false)) {
        return AttachmentEncodingMode::kUuencode;
    }
    return AttachmentEncodingMode::kMime;
}

}  // namespace

ComposePolicy ComposePolicyFromSettings(const SettingsStore& settings, std::string_view section) {
    ComposePolicy policy;
    policy.allow_styled = settings.GetBool(section, "AllowStyledCompose", true);
    policy.default_stationery_name =
        NormalizeLegacyDefaultValue(settings.GetString(section, "Stationery").value_or(""));
    policy.user_signatures_enabled = settings.GetBool(section, "UserSignatures", false);
    policy.default_signature_name =
        NormalizeLegacyDefaultValue(settings.GetString(section, "Signature").value_or(""));
    policy.send_plain_and_styled = settings.GetBool(section, "SendPlainAndStyled", true);
    policy.send_styled_only = settings.GetBool(section, "SendStyledOnly", false);
    policy.send_plain_only = !policy.send_plain_and_styled && !policy.send_styled_only;
    policy.warn_on_styled_send = settings.GetBool(section, "WarnQueueStyledText", false);

    policy.mood_watch_enabled = settings.GetBool(section, "DoMoodWatchCheck", true);
    policy.mood_check_background = settings.GetBool(section, "MoodCheckBackground", true);
    policy.mood_watch_interval_ms = settings.GetInt(section, "MoodWatchInterval", 1000);
    policy.mood_warn_when_might_offend =
        settings.GetBool(section, "MoodWatchWarnWhenMightBeOffensive", false);
    policy.mood_warn_when_probably_offend =
        settings.GetBool(section, "MoodWatchWarnWhenProbablyOffensive", true);
    policy.mood_warn_when_on_fire = settings.GetBool(section, "MoodWatchWarnFire", false);
    policy.mood_show_comp_bad_words = settings.GetBool(section, "MoodShowCompBadWords", true);

    policy.auto_check_spelling = settings.GetBool(section, "AutoSpellCheck", true);
    policy.spell_check_interval_ms = settings.GetInt(section, "SpellCheckInterval", 500);
    policy.boss_protector_interval_ms = settings.GetInt(section, "BossProtectorInterval", 500);
    policy.boss_protector_warn_outside_domains =
        settings.GetBool(section, "BPWarnOutsideDomains", false);
    policy.boss_protector_outside_domains =
        settings.GetString(section, "BPOutsideDomains").value_or("");
    policy.boss_protector_warn_inside_domains =
        settings.GetBool(section, "BPWarnInsideDomains", false);
    policy.boss_protector_inside_domains =
        settings.GetString(section, "BPInsideDomains").value_or("");
    policy.boss_protector_additional_warn_dialog =
        settings.GetBool(section, "BPAdditionalWarnDialog", false);
    policy.default_options.attachment_encoding = AttachmentEncodingFromSettings(settings, section);
    policy.default_options.quoted_printable = settings.GetBool(section, "UseQP", true);
    policy.default_options.word_wrap = settings.GetBool(section, "WordWrap", true);
    policy.default_options.tabs_in_body = settings.GetBool(section, "TabsInBody", true);
    policy.default_options.keep_copies = settings.GetBool(section, "KeepCopies", true);
    policy.default_options.request_read_receipt = false;
    policy.default_options.text_as_document = false;
    policy.return_receipt_legacy_header = settings.GetBool(section, "ReturnReceiptFlag", false);
    policy.word_wrap_on_screen = settings.GetBool(section, "WordWrapOnScreen", false);
    policy.word_wrap_column = std::max(20, settings.GetInt(section, "WordWrapColumn", 70));
    policy.word_wrap_max = std::max(policy.word_wrap_column, settings.GetInt(section, "WordWrapMax", 80));

    if (policy.read_only) {
        policy.allow_styled = false;
    }

    return policy;
}

StyledSendMode ResolveStyledSendMode(const ComposePolicy& policy, bool has_styles) {
    if (!has_styles || !policy.allow_styled) {
        return StyledSendMode::kPlainTextOnly;
    }

    if (policy.send_styled_only) {
        return StyledSendMode::kStyledTextOnly;
    }
    if (policy.send_plain_and_styled) {
        return StyledSendMode::kPlainAndStyled;
    }
    return StyledSendMode::kPlainTextOnly;
}

}  // namespace hermes
