#pragma once

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/SettingsStore.h"

namespace hermes {

enum class TransportSecurityMode {
    kPlaintext,
    kImplicitTls,
    kStartTls,
};

enum class PopAuthMode {
    kPassword,
    kKerberos,
    kAPOP,
    kRPA,
};

enum class ImapAuthMode {
    kPassword,
    kKerberos,
    kCramMd5,
};

enum class SmtpAuthMode {
    kNone,
    kCramMd5,
    kLogin,
    kPlain,
};

struct KerberosSettings {
    std::string service_name;
    std::string realm;
    std::string service_format;
    std::uint16_t pop_port = 110;
};

struct AccountProfile {
    std::string id;
    std::string display_name;
    std::string email_address;
    std::string login_name;
    std::string incoming_server;
    std::string outgoing_server;
    std::uint16_t incoming_port = 0;
    std::uint16_t outgoing_port = 0;
    TransportSecurityMode incoming_security = TransportSecurityMode::kPlaintext;
    TransportSecurityMode outgoing_security = TransportSecurityMode::kPlaintext;
    PopAuthMode pop_auth = PopAuthMode::kPassword;
    ImapAuthMode imap_auth = ImapAuthMode::kPassword;
    SmtpAuthMode smtp_auth = SmtpAuthMode::kNone;
    bool uses_pop = false;
    bool uses_imap = false;
    bool smtp_auth_allowed = false;
    bool delete_mail_from_server = false;
    bool leave_mail_on_server = false;
    bool skip_big_messages = false;
    bool check_mail_by_default = false;
    std::size_t big_message_threshold = 0;
    std::size_t imap_max_download_size = 0;
    bool imap_omit_attachments = false;
    bool transfer_to_trash_on_delete = false;
    std::string imap_directory_prefix;
    std::string trash_mailbox_name;
    KerberosSettings kerberos;
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
