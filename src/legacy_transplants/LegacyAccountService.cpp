#include "hermes/AccountService.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>

#include "hermes/IniSettingsStore.h"

namespace hermes {

namespace {

std::uint16_t ParsePort(const SettingsStore& settings,
                        std::string_view section,
                        std::string_view key,
                        std::uint16_t fallback) {
    const int value = settings.GetInt(section, key, static_cast<int>(fallback));
    if (value <= 0 || value > 65535) {
        return fallback;
    }
    return static_cast<std::uint16_t>(value);
}

TransportSecurityMode SecurityModeFromSettings(const SettingsStore& settings,
                                               std::string_view section,
                                               std::string_view prefix,
                                               std::uint16_t port) {
    const auto explicit_mode = settings.GetString(section, std::string(prefix) + "Security");
    if (explicit_mode) {
        std::string normalized = *explicit_mode;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (normalized == "starttls") {
            return TransportSecurityMode::kStartTls;
        }
        if (normalized == "ssl" || normalized == "tls" || normalized == "implicit") {
            return TransportSecurityMode::kImplicitTls;
        }
    }

    if (settings.GetBool(section, std::string(prefix) + "StartTls", false) ||
        settings.GetBool(section, std::string(prefix) + "STARTTLS", false)) {
        return TransportSecurityMode::kStartTls;
    }

    if (settings.GetBool(section, std::string(prefix) + "SSLUse", false)) {
        if (port == 465 || port == 993 || port == 995) {
            return TransportSecurityMode::kImplicitTls;
        }
        return TransportSecurityMode::kStartTls;
    }

    return TransportSecurityMode::kPlaintext;
}

PopAuthMode PopAuthFromSettings(const SettingsStore& settings, std::string_view section) {
    if (settings.GetBool(section, "AuthenticateKerberos", false)) {
        return PopAuthMode::kKerberos;
    }
    if (settings.GetBool(section, "AuthenticateAPOP", false)) {
        return PopAuthMode::kAPOP;
    }
    if (settings.GetBool(section, "AuthenticateRPA", false)) {
        return PopAuthMode::kRPA;
    }
    return PopAuthMode::kPassword;
}

ImapAuthMode ImapAuthFromSettings(const SettingsStore& settings, std::string_view section) {
    if (settings.GetBool(section, "AuthenticateKerberos", false)) {
        return ImapAuthMode::kKerberos;
    }
    if (settings.GetBool(section, "AuthenticateCRAMMD5", false) ||
        settings.GetBool(section, "AuthenticateCramMd5", false)) {
        return ImapAuthMode::kCramMd5;
    }
    return ImapAuthMode::kPassword;
}

SmtpAuthMode SmtpAuthFromSettings(const SettingsStore& settings, std::string_view section) {
    const auto method = settings.GetString(section, "SmtpAuthMethod");
    if (method) {
        std::string normalized = *method;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (normalized == "cram-md5" || normalized == "cram_md5") {
            return SmtpAuthMode::kCramMd5;
        }
        if (normalized == "login") {
            return SmtpAuthMode::kLogin;
        }
        if (normalized == "plain") {
            return SmtpAuthMode::kPlain;
        }
        if (normalized == "none") {
            return SmtpAuthMode::kNone;
        }
    }

    if (settings.GetBool(section, "SmtpAuthAllowed", false)) {
        return SmtpAuthMode::kPlain;
    }
    return SmtpAuthMode::kNone;
}

ImapDownloadMode ImapDownloadModeFromSettings(const SettingsStore& settings,
                                              std::string_view section,
                                              bool omit_attachments,
                                              std::size_t max_download_size) {
    if (settings.GetBool(section, "IMAPMinDownload", false)) {
        return ImapDownloadMode::kMinimalHeaders;
    }
    if (omit_attachments || max_download_size > 0) {
        return ImapDownloadMode::kMessageBody;
    }
    return ImapDownloadMode::kFullMessage;
}

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
    profile.uses_pop = settings.GetBool(section, "UsesPOP", false);
    profile.uses_imap = settings.GetBool(section, "UsesIMAP", false);
    profile.incoming_server = settings.GetString(section, profile.uses_imap ? "ImapServer" : "PopServer")
                                  .value_or(settings.GetString(section, "PopServer").value_or(""));
    profile.outgoing_server = settings.GetString(section, "SMTPServer").value_or("");
    profile.incoming_port =
        profile.uses_imap ? ParsePort(settings, section, "IMAPPort", 143) : ParsePort(settings, section, "POPPort", 110);
    profile.outgoing_port = ParsePort(settings, section, "SMTPPort", 25);
    profile.incoming_security =
        SecurityModeFromSettings(settings, section, profile.uses_imap ? "IMAP" : "POP", profile.incoming_port);
    profile.outgoing_security = SecurityModeFromSettings(settings, section, "SMTP", profile.outgoing_port);
    profile.pop_auth = PopAuthFromSettings(settings, section);
    profile.imap_auth = ImapAuthFromSettings(settings, section);
    profile.smtp_auth = SmtpAuthFromSettings(settings, section);
    profile.smtp_auth_allowed = settings.GetBool(section, "SmtpAuthAllowed", false);
    profile.leave_mail_on_server = settings.GetBool(section, "LeaveMailOnServer", false);
    profile.delete_mail_from_server = settings.GetBool(section, "DeleteMailFromServer", false);
    profile.skip_big_messages = settings.GetBool(section, "SkipBigMessages", false);
    profile.check_mail_by_default = settings.GetBool(section, "CheckMailByDefault", false);
    profile.big_message_threshold =
        static_cast<std::size_t>(std::max(settings.GetInt(section, "BigMessageThreshold", 0), 0));
    profile.imap_max_download_size =
        static_cast<std::size_t>(std::max(settings.GetInt(section, "IMAPMaxDownloadSize", 0), 0));
    profile.imap_omit_attachments = settings.GetBool(section, "IMAPOmitAttachments", false);
    profile.mark_as_deleted = settings.GetBool(section, "MarkAsDeleted", false);
    profile.transfer_to_trash_on_delete = settings.GetBool(section, "TransferToTrashOnDelete", false);
    profile.imap_download_mode = ImapDownloadModeFromSettings(settings,
                                                              section,
                                                              profile.imap_omit_attachments,
                                                              profile.imap_max_download_size);
    profile.imap_directory_prefix = settings.GetString(section, "ImapDirectoryPrefix").value_or("");
    profile.trash_mailbox_name = settings.GetString(section, "TrashMailboxName").value_or("Trash");
    profile.kerberos.service_name =
        settings.GetString(section, "KerberosServiceName").value_or(profile.uses_imap ? "imap" : "pop");
    profile.kerberos.realm = settings.GetString(section, "KerberosRealm").value_or("");
    profile.kerberos.service_format =
        settings.GetString(section, "KerberosServiceFormat").value_or("%s@%h");
    profile.kerberos.pop_port =
        ParsePort(settings, section, "KerberosPOPPort", profile.incoming_port == 0 ? 110 : profile.incoming_port);
    return profile;
}

std::string SecurityModeToString(TransportSecurityMode mode) {
    switch (mode) {
        case TransportSecurityMode::kPlaintext:
            return "plain";
        case TransportSecurityMode::kImplicitTls:
            return "ssl";
        case TransportSecurityMode::kStartTls:
            return "starttls";
    }
    return "plain";
}

void WriteBool(SettingsStore& settings, std::string_view section, std::string_view key, bool value) {
    settings.SetString(section, key, value ? "1" : "0");
}

void WriteInt(SettingsStore& settings, std::string_view section, std::string_view key, std::uint16_t value) {
    settings.SetString(section, key, std::to_string(value));
}

void WriteSize(SettingsStore& settings, std::string_view section, std::string_view key, std::size_t value) {
    settings.SetString(section, key, std::to_string(static_cast<unsigned long long>(value)));
}

std::string StorageSectionName(const AccountProfile& account, std::size_t index) {
    if (index == 0) {
        return "Settings";
    }
    if (account.id.empty() || account.id == "Settings") {
        return "Persona-" + std::to_string(index);
    }
    return account.id;
}

void ClearAccountSection(SettingsStore& settings, std::string_view section) {
    static constexpr const char* kAccountKeys[] = {
        "RealName",
        "ReturnAddress",
        "POPAccount",
        "LoginName",
        "UsesPOP",
        "UsesIMAP",
        "PopServer",
        "ImapServer",
        "SMTPServer",
        "POPPort",
        "IMAPPort",
        "SMTPPort",
        "POPSecurity",
        "IMAPSecurity",
        "SMTPSecurity",
        "POPSSLUse",
        "IMAPSSLUse",
        "SMTPSSLUse",
        "POPStartTls",
        "IMAPStartTls",
        "SMTPStartTls",
        "AuthenticateKerberos",
        "AuthenticateAPOP",
        "AuthenticateRPA",
        "AuthenticateCRAMMD5",
        "SmtpAuthAllowed",
        "SmtpAuthMethod",
        "LeaveMailOnServer",
        "DeleteMailFromServer",
        "SkipBigMessages",
        "CheckMailByDefault",
        "BigMessageThreshold",
        "IMAPMaxDownloadSize",
        "IMAPOmitAttachments",
        "MarkAsDeleted",
        "TransferToTrashOnDelete",
        "IMAPMinDownload",
        "ImapDirectoryPrefix",
        "TrashMailboxName",
        "KerberosServiceName",
        "KerberosRealm",
        "KerberosServiceFormat",
        "KerberosPOPPort",
    };

    for (const char* key : kAccountKeys) {
        settings.RemoveValue(section, key);
    }
}

void WriteAccount(SettingsStore& settings, std::string_view section, const AccountProfile& profile) {
    settings.SetString(section, "RealName", profile.display_name);
    settings.SetString(section,
                       "ReturnAddress",
                       profile.email_address.empty() ? profile.id : profile.email_address);
    settings.SetString(section,
                       "POPAccount",
                       profile.email_address.empty() ? profile.id : profile.email_address);
    settings.SetString(section, "LoginName", profile.login_name);
    WriteBool(settings, section, "UsesPOP", profile.uses_pop);
    WriteBool(settings, section, "UsesIMAP", profile.uses_imap);
    settings.SetString(section, "PopServer", profile.incoming_server);
    settings.SetString(section, "ImapServer", profile.incoming_server);
    settings.SetString(section, "SMTPServer", profile.outgoing_server);
    WriteInt(settings, section, "POPPort", profile.incoming_port == 0 ? 110 : profile.incoming_port);
    WriteInt(settings, section, "IMAPPort", profile.incoming_port == 0 ? 143 : profile.incoming_port);
    WriteInt(settings, section, "SMTPPort", profile.outgoing_port == 0 ? 25 : profile.outgoing_port);
    settings.SetString(section,
                       profile.uses_imap ? "IMAPSecurity" : "POPSecurity",
                       SecurityModeToString(profile.incoming_security));
    settings.SetString(section, "SMTPSecurity", SecurityModeToString(profile.outgoing_security));
    WriteBool(settings,
              section,
              profile.uses_imap ? "IMAPSSLUse" : "POPSSLUse",
              profile.incoming_security != TransportSecurityMode::kPlaintext);
    WriteBool(settings,
              section,
              profile.uses_imap ? "IMAPStartTls" : "POPStartTls",
              profile.incoming_security == TransportSecurityMode::kStartTls);
    WriteBool(settings,
              section,
              "SMTPSSLUse",
              profile.outgoing_security != TransportSecurityMode::kPlaintext);
    WriteBool(settings,
              section,
              "SMTPStartTls",
              profile.outgoing_security == TransportSecurityMode::kStartTls);
    WriteBool(settings, section, "AuthenticateAPOP", profile.pop_auth == PopAuthMode::kAPOP);
    WriteBool(settings, section, "AuthenticateRPA", profile.pop_auth == PopAuthMode::kRPA);
    WriteBool(settings,
              section,
              "AuthenticateKerberos",
              profile.pop_auth == PopAuthMode::kKerberos || profile.imap_auth == ImapAuthMode::kKerberos);
    WriteBool(settings, section, "AuthenticateCRAMMD5", profile.imap_auth == ImapAuthMode::kCramMd5);
    WriteBool(settings, section, "SmtpAuthAllowed", profile.smtp_auth_allowed);
    switch (profile.smtp_auth) {
        case SmtpAuthMode::kNone:
            settings.SetString(section, "SmtpAuthMethod", "none");
            break;
        case SmtpAuthMode::kCramMd5:
            settings.SetString(section, "SmtpAuthMethod", "cram-md5");
            break;
        case SmtpAuthMode::kLogin:
            settings.SetString(section, "SmtpAuthMethod", "login");
            break;
        case SmtpAuthMode::kPlain:
            settings.SetString(section, "SmtpAuthMethod", "plain");
            break;
    }
    WriteBool(settings, section, "LeaveMailOnServer", profile.leave_mail_on_server);
    WriteBool(settings, section, "DeleteMailFromServer", profile.delete_mail_from_server);
    WriteBool(settings, section, "SkipBigMessages", profile.skip_big_messages);
    WriteBool(settings, section, "CheckMailByDefault", profile.check_mail_by_default);
    WriteSize(settings, section, "BigMessageThreshold", profile.big_message_threshold);
    WriteSize(settings, section, "IMAPMaxDownloadSize", profile.imap_max_download_size);
    WriteBool(settings, section, "IMAPOmitAttachments", profile.imap_omit_attachments);
    WriteBool(settings, section, "MarkAsDeleted", profile.mark_as_deleted);
    WriteBool(settings, section, "TransferToTrashOnDelete", profile.transfer_to_trash_on_delete);
    WriteBool(settings,
              section,
              "IMAPMinDownload",
              profile.imap_download_mode == ImapDownloadMode::kMinimalHeaders);
    settings.SetString(section, "ImapDirectoryPrefix", profile.imap_directory_prefix);
    settings.SetString(section, "TrashMailboxName", profile.trash_mailbox_name);
    settings.SetString(section, "KerberosServiceName", profile.kerberos.service_name);
    settings.SetString(section, "KerberosRealm", profile.kerberos.realm);
    settings.SetString(section, "KerberosServiceFormat", profile.kerberos.service_format);
    WriteInt(settings, section, "KerberosPOPPort", profile.kerberos.pop_port);
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

void LegacyAccountService::SetAccounts(std::vector<AccountProfile> accounts) {
    accounts_ = std::move(accounts);
}

void LegacyAccountService::AddOrReplace(const AccountProfile& account) {
    const auto it = std::find_if(accounts_.begin(), accounts_.end(), [&](const AccountProfile& existing) {
        return existing.id == account.id;
    });
    if (it != accounts_.end()) {
        *it = account;
        return;
    }
    accounts_.push_back(account);
}

bool LegacyAccountService::Remove(std::string_view id) {
    const auto it = std::remove_if(accounts_.begin(), accounts_.end(), [&](const AccountProfile& account) {
        return account.id == id;
    });
    if (it == accounts_.end()) {
        return false;
    }
    accounts_.erase(it, accounts_.end());
    return true;
}

bool LegacyAccountService::SaveToSettings(SettingsStore& settings, std::string* error_message) const {
    for (const auto& section : settings.Sections()) {
        if (LooksLikeAccountSection(settings, section)) {
            ClearAccountSection(settings, section);
        }
    }

    for (std::size_t index = 0; index < accounts_.size(); ++index) {
        WriteAccount(settings, StorageSectionName(accounts_[index], index), accounts_[index]);
    }

    if (error_message) {
        error_message->clear();
    }
    return true;
}

bool LegacyAccountService::SaveToIniFile(const std::filesystem::path& path, std::string* error_message) const {
    IniSettingsStore settings;
    for (std::size_t index = 0; index < accounts_.size(); ++index) {
        WriteAccount(settings, StorageSectionName(accounts_[index], index), accounts_[index]);
    }
    return settings.SaveToFile(path, error_message);
}

}  // namespace hermes
