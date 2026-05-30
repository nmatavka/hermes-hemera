#include "hermes/MailTransferSettings.h"

namespace hermes {

namespace {

TransferPersonaOptionsMode TransferPersonaOptionsModeFromInt(int value) {
    return value == 0 ? TransferPersonaOptionsMode::kSpecifiedOptions
                      : TransferPersonaOptionsMode::kNormalOptions;
}

int TransferPersonaOptionsModeToInt(TransferPersonaOptionsMode value) {
    switch (value) {
        case TransferPersonaOptionsMode::kSpecifiedOptions:
            return 0;
        case TransferPersonaOptionsMode::kNormalOptions:
            return 1;
    }
    return 1;
}

}  // namespace

MailTransferSettings MailTransferSettingsFromSettings(const SettingsStore& settings,
                                                      std::string_view section) {
    MailTransferSettings transfer_settings;
    transfer_settings.immediate_send = settings.GetBool(section, "ImmediateSend", true);
    transfer_settings.send_on_check = settings.GetBool(section, "SendOnCheck", true);
    transfer_settings.leave_mail_on_server = settings.GetBool(section, "LeaveMailOnServer", false);
    transfer_settings.transfer_persona_options = TransferPersonaOptionsModeFromInt(
        settings.GetInt(section, "TransferPersonaOptions", 1));
    return transfer_settings;
}

void ApplyMailTransferSettingsToSettings(const MailTransferSettings& transfer_settings,
                                         SettingsStore& settings,
                                         std::string_view section) {
    settings.SetString(section, "ImmediateSend", transfer_settings.immediate_send ? "1" : "0");
    settings.SetString(section, "SendOnCheck", transfer_settings.send_on_check ? "1" : "0");
    settings.SetString(
        section, "LeaveMailOnServer", transfer_settings.leave_mail_on_server ? "1" : "0");
    settings.SetString(section,
                       "TransferPersonaOptions",
                       std::to_string(TransferPersonaOptionsModeToInt(
                           transfer_settings.transfer_persona_options)));
}

}  // namespace hermes
