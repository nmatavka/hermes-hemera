#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hermes/SettingsStore.h"

namespace hermes {

enum class TransferPersonaOptionsMode {
    kSpecifiedOptions = 0,
    kNormalOptions = 1,
};

struct MailTransferSettings {
    bool immediate_send = true;
    bool send_on_check = true;
    bool leave_mail_on_server = false;
    TransferPersonaOptionsMode transfer_persona_options = TransferPersonaOptionsMode::kNormalOptions;
};

struct MailTransferRequest {
    bool send_queued = false;
    bool retrieve_new = false;
    bool delete_marked = false;
    bool retrieve_marked = false;
    bool delete_retrieved = false;
    bool delete_all = false;
    bool fetch_headers = false;
    bool ignore_check_mail_by_default = false;
    std::vector<std::string> selected_account_ids;
};

MailTransferSettings MailTransferSettingsFromSettings(const SettingsStore& settings,
                                                      std::string_view section = "Settings");
void ApplyMailTransferSettingsToSettings(const MailTransferSettings& transfer_settings,
                                         SettingsStore& settings,
                                         std::string_view section = "Settings");

}  // namespace hermes
