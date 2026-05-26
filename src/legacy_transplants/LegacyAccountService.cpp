#include "hermes/AccountService.h"

#include <algorithm>

#include "hermes/IniSettingsStore.h"

namespace hermes {

namespace {

bool LooksLikeAccountSection(const SettingsStore& settings, std::string_view section) {
    static constexpr const char* kKeys[] = {
        "POPAccount",
        "ReturnAddress",
        "SMTPServer",
        "PopServer",
        "LoginName",
    };

    for (const char* key : kKeys) {
        if (settings.HasValue(section, key)) {
            return true;
        }
    }
    return false;
}

AccountProfile ProfileFromSection(const SettingsStore& settings, std::string_view section) {
    AccountProfile profile;
    const auto fallback_id = settings.GetString(section, "POPAccount")
                                 .value_or(settings.GetString(section, "LoginName").value_or("primary"));
    profile.id = section == "Settings" ? fallback_id : std::string(section);
    profile.display_name = settings.GetString(section, "RealName").value_or(profile.id);
    profile.email_address = settings.GetString(section, "ReturnAddress")
                                .value_or(settings.GetString(section, "POPAccount").value_or(""));
    profile.login_name = settings.GetString(section, "LoginName").value_or("");
    profile.incoming_server = settings.GetString(section, "PopServer").value_or("");
    profile.outgoing_server = settings.GetString(section, "SMTPServer").value_or("");
    profile.uses_pop = settings.GetBool(section, "UsesPOP", false);
    profile.uses_imap = settings.GetBool(section, "UsesIMAP", false);
    profile.smtp_auth_allowed = settings.GetBool(section, "SmtpAuthAllowed", false);
    profile.leave_mail_on_server = settings.GetBool(section, "LeaveMailOnServer", false);
    return profile;
}

}  // namespace

bool LegacyAccountService::LoadFromSettings(const SettingsStore& settings) {
    accounts_.clear();
    for (const auto& section : settings.Sections()) {
        if (!LooksLikeAccountSection(settings, section)) {
            continue;
        }
        accounts_.push_back(ProfileFromSection(settings, section));
    }
    return !accounts_.empty();
}

bool LegacyAccountService::LoadFromIniFile(const std::filesystem::path& path, std::string* error_message) {
    IniSettingsStore settings;
    if (!settings.LoadFromFile(path, error_message)) {
        return false;
    }
    if (LoadFromSettings(settings)) {
        return true;
    }

    if (error_message) {
        *error_message = "No account settings were found in " + path.string();
    }
    return false;
}

std::vector<AccountProfile> LegacyAccountService::Accounts() const {
    return accounts_;
}

std::optional<AccountProfile> LegacyAccountService::FindById(std::string_view id) const {
    const auto it = std::find_if(accounts_.begin(), accounts_.end(), [&](const AccountProfile& account) {
        return account.id == id;
    });
    if (it == accounts_.end()) {
        return std::nullopt;
    }
    return *it;
}

}  // namespace hermes
