#pragma once

#include <string>
#include <string_view>

#include "hermes/RichTextSurface.h"
#include "hermes/SettingsStore.h"

namespace hermes {

enum class StyledSendMode {
    kPlainTextOnly,
    kPlainAndStyled,
    kStyledTextOnly,
};

struct ComposeHeaders {
    std::string to;
    std::string cc;
    std::string bcc;
    std::string subject;
    std::string from_persona;
    std::string reply_to;
};

struct ComposePolicy {
    bool read_only = false;
    bool allow_styled = true;
    bool send_plain_only = false;
    bool warn_on_styled_send = false;
    std::string default_signature_name;
    std::string default_stationery_name;
    bool user_signatures_enabled = false;
    bool mood_watch_enabled = true;
    bool mood_check_background = true;
    int mood_watch_interval_ms = 1000;
    bool mood_warn_when_might_offend = false;
    bool mood_warn_when_probably_offend = true;
    bool mood_warn_when_on_fire = false;
    bool mood_show_comp_bad_words = true;
    bool auto_check_spelling = true;
    int spell_check_interval_ms = 500;
    int boss_protector_interval_ms = 500;
    bool boss_protector_warn_outside_domains = false;
    std::string boss_protector_outside_domains;
    bool boss_protector_warn_inside_domains = false;
    std::string boss_protector_inside_domains;
    bool boss_protector_additional_warn_dialog = false;
    bool send_plain_and_styled = true;
    bool send_styled_only = false;
};

struct ManagedSignatureBlock {
    bool attached = false;
    std::string name;
    std::size_t start = 0;
    std::size_t length = 0;
    std::string plain_text;
};

struct ComposeMessage {
    std::string id;
    ComposeHeaders headers;
    ComposePolicy policy;
    RichTextDocument body;
    std::string stationery_name;
    std::string signature_name;
    ManagedSignatureBlock managed_signature;
};

ComposePolicy ComposePolicyFromSettings(const SettingsStore& settings, std::string_view section = "Settings");
StyledSendMode ResolveStyledSendMode(const ComposePolicy& policy, bool has_styles);

}  // namespace hermes
