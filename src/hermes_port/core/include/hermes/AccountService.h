#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/SettingsStore.h"

namespace hermes {

struct AccountProfile {
    std::string id;
    std::string display_name;
    std::string email_address;
    std::string login_name;
    std::string incoming_server;
    std::string outgoing_server;
    bool uses_pop = false;
    bool uses_imap = false;
    bool smtp_auth_allowed = false;
    bool leave_mail_on_server = false;
};

class AccountService {
public:
    virtual ~AccountService() = default;

    virtual std::vector<AccountProfile> Accounts() const = 0;
    virtual std::optional<AccountProfile> FindById(std::string_view id) const = 0;
};

class LegacyAccountService final : public AccountService {
public:
    bool LoadFromSettings(const SettingsStore& settings);
    bool LoadFromIniFile(const std::filesystem::path& path, std::string* error_message = nullptr);

    std::vector<AccountProfile> Accounts() const override;
    std::optional<AccountProfile> FindById(std::string_view id) const override;

private:
    std::vector<AccountProfile> accounts_;
};

}  // namespace hermes
