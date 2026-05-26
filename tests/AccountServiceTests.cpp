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
    HERMES_CHECK(account.uses_pop);
    HERMES_CHECK(!account.uses_imap);
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
    settings.SetString("Persona-Work", "PopServer", "imap.example.com");
    settings.SetString("Persona-Work", "SMTPServer", "smtp.work.example.com");
    settings.SetString("Persona-Work", "UsesIMAP", "1");

    hermes::LegacyAccountService service;
    HERMES_CHECK(service.LoadFromSettings(settings));
    HERMES_CHECK_EQ(service.Accounts().size(), static_cast<std::size_t>(2));

    const auto work = service.FindById("Persona-Work");
    HERMES_CHECK(static_cast<bool>(work));
    HERMES_CHECK_EQ(work->email_address, std::string("work@example.com"));
    HERMES_CHECK(work->uses_imap);
}
