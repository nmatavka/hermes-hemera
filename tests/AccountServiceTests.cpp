#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/AccountService.h"
#include "hermes/IniSettingsStore.h"

HERMES_TEST(LegacyAccountServiceLoadsTrackedProfileSnapshot) {
    hermes::LegacyAccountService service;
    std::string error_message;
    HERMES_CHECK(service.LoadFromIniFile(
        hermes::tests::FixtureRoot() / "profile_snapshots" / "Eudora.box",
        &error_message));

    const auto accounts = service.Accounts();
    HERMES_CHECK_EQ(accounts.size(), static_cast<std::size_t>(1));

    const auto account = accounts.front();
    HERMES_CHECK_EQ(account.display_name, std::string("Jason"));
    HERMES_CHECK_EQ(account.email_address, std::string("jmiller@swamp.qualcomm.com"));
    HERMES_CHECK_EQ(account.login_name, std::string("jmiller"));
    HERMES_CHECK_EQ(account.incoming_server, std::string("swamp.qualcomm.com"));
    HERMES_CHECK_EQ(account.outgoing_server, std::string("swamp.qualcomm.com"));
    HERMES_CHECK_EQ(account.incoming_port, static_cast<std::uint16_t>(110));
    HERMES_CHECK_EQ(account.outgoing_port, static_cast<std::uint16_t>(25));
    HERMES_CHECK(account.uses_pop);
    HERMES_CHECK(!account.uses_imap);
    HERMES_CHECK_EQ(account.pop_auth, hermes::PopAuthMode::kPassword);
    HERMES_CHECK_EQ(account.smtp_auth, hermes::SmtpAuthMode::kNone);
    HERMES_CHECK(account.check_mail_by_default);
    HERMES_CHECK_EQ(account.big_message_threshold, static_cast<std::size_t>(40960));
    HERMES_CHECK_EQ(account.trash_mailbox_name, std::string("Trash"));
}

HERMES_TEST(LegacyAccountServiceCanLoadMultipleAccountSections) {
    hermes::IniSettingsStore settings;
    settings.SetString("Settings", "RealName", "Primary");
    settings.SetString("Settings", "POPAccount", "primary@example.com");
    settings.SetString("Settings", "PopServer", "pop.example.com");
    settings.SetString("Settings", "SMTPServer", "smtp.example.com");
    settings.SetString("Persona-Work", "RealName", "Work");
    settings.SetString("Persona-Work", "ReturnAddress", "work@example.com");
    settings.SetString("Persona-Work", "LoginName", "work");
    settings.SetString("Persona-Work", "ImapServer", "imap.example.com");
    settings.SetString("Persona-Work", "SMTPServer", "smtp.work.example.com");
    settings.SetString("Persona-Work", "UsesIMAP", "1");
    settings.SetString("Persona-Work", "IMAPPort", "993");
    settings.SetString("Persona-Work", "IMAPSSLUse", "1");
    settings.SetString("Persona-Work", "AuthenticateCRAMMD5", "1");
    settings.SetString("Persona-Work", "SmtpAuthAllowed", "1");

    hermes::LegacyAccountService service;
    HERMES_CHECK(service.LoadFromSettings(settings));
    HERMES_CHECK_EQ(service.Accounts().size(), static_cast<std::size_t>(2));

    const auto work = service.FindById("Persona-Work");
    HERMES_CHECK(static_cast<bool>(work));
    HERMES_CHECK_EQ(work->email_address, std::string("work@example.com"));
    HERMES_CHECK(work->uses_imap);
    HERMES_CHECK_EQ(work->incoming_server, std::string("imap.example.com"));
    HERMES_CHECK_EQ(work->incoming_port, static_cast<std::uint16_t>(993));
    HERMES_CHECK_EQ(work->incoming_security, hermes::TransportSecurityMode::kImplicitTls);
    HERMES_CHECK_EQ(work->imap_auth, hermes::ImapAuthMode::kCramMd5);
    HERMES_CHECK_EQ(work->smtp_auth, hermes::SmtpAuthMode::kPlain);
}

HERMES_TEST(LegacyAccountServiceProjectsImapDeletionAndDownloadSettings) {
    hermes::IniSettingsStore settings;
    settings.SetString("Settings", "RealName", "IMAP");
    settings.SetString("Settings", "ReturnAddress", "imap@example.com");
    settings.SetString("Settings", "LoginName", "imap");
    settings.SetString("Settings", "UsesIMAP", "1");
    settings.SetString("Settings", "ImapServer", "imap.example.com");
    settings.SetString("Settings", "SMTPServer", "smtp.example.com");
    settings.SetString("Settings", "IMAPMinDownload", "1");
    settings.SetString("Settings", "IMAPOmitAttachments", "1");
    settings.SetString("Settings", "MarkAsDeleted", "1");
    settings.SetString("Settings", "TransferToTrashOnDelete", "0");

    hermes::LegacyAccountService service;
    HERMES_CHECK(service.LoadFromSettings(settings));
    const auto account = service.Accounts().front();
    HERMES_CHECK(account.mark_as_deleted);
    HERMES_CHECK(account.imap_omit_attachments);
    HERMES_CHECK_EQ(account.imap_download_mode, hermes::ImapDownloadMode::kMinimalHeaders);
}

HERMES_TEST(LegacyAccountServiceCanRoundTripWritablePersonalities) {
    hermes::LegacyAccountService service;
    hermes::AccountProfile primary;
    primary.id = "primary@example.com";
    primary.display_name = "Primary";
    primary.email_address = "primary@example.com";
    primary.login_name = "primary";
    primary.incoming_server = "pop.example.com";
    primary.outgoing_server = "smtp.example.com";
    primary.incoming_port = 995;
    primary.outgoing_port = 465;
    primary.uses_pop = true;
    primary.incoming_security = hermes::TransportSecurityMode::kImplicitTls;
    primary.outgoing_security = hermes::TransportSecurityMode::kImplicitTls;

    hermes::AccountProfile work;
    work.id = "Persona-Work";
    work.display_name = "Work";
    work.email_address = "work@example.com";
    work.login_name = "work";
    work.incoming_server = "imap.example.com";
    work.outgoing_server = "smtp.work.example.com";
    work.incoming_port = 993;
    work.outgoing_port = 587;
    work.uses_imap = true;
    work.incoming_security = hermes::TransportSecurityMode::kImplicitTls;
    work.outgoing_security = hermes::TransportSecurityMode::kStartTls;
    work.imap_auth = hermes::ImapAuthMode::kKerberos;
    work.smtp_auth = hermes::SmtpAuthMode::kLogin;
    work.smtp_auth_allowed = true;
    work.kerberos.service_name = "imap";
    work.kerberos.realm = "EXAMPLE.COM";

    service.SetAccounts({primary});
    service.AddOrReplace(work);
    HERMES_CHECK(service.Remove("missing") == false);

    hermes::IniSettingsStore settings;
    std::string error_message;
    HERMES_CHECK(service.SaveToSettings(settings, &error_message));

    hermes::LegacyAccountService reloaded;
    HERMES_CHECK(reloaded.LoadFromSettings(settings));
    HERMES_CHECK_EQ(reloaded.Accounts().size(), static_cast<std::size_t>(2));

    const auto saved_work = reloaded.FindById("Persona-Work");
    HERMES_CHECK(static_cast<bool>(saved_work));
    HERMES_CHECK(saved_work->uses_imap);
    HERMES_CHECK_EQ(saved_work->imap_auth, hermes::ImapAuthMode::kKerberos);
    HERMES_CHECK_EQ(saved_work->outgoing_security, hermes::TransportSecurityMode::kStartTls);
}
