#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/AddressBookService.h"
#include "hermes/DirectoryServiceCatalog.h"
#include "hermes/FilterReportStore.h"
#include "hermes/FilterStore.h"
#include "hermes/LinkHistoryStore.h"
#include "hermes/MailTaskModel.h"
#include "hermes/NicknameStore.h"
#include "hermes/StationeryStore.h"

HERMES_TEST(FilesystemFilterStoreRoundTripsRules) {
    hermes::tests::ScopedTempDirectory temp("hemera-filters");
    const auto path = temp.Path() / "filters.ini";

    hermes::FilesystemFilterStore store;
    store.AddOrReplace({"vip",
                        hermes::FilterField::kFrom,
                        hermes::FilterOperation::kContains,
                        "vip@example.com",
                        std::string("Priority"),
                        true,
                        false,
                        true});
    std::string error_message;
    HERMES_CHECK(store.SaveToFile(path, &error_message));

    hermes::FilesystemFilterStore loaded;
    HERMES_CHECK(loaded.LoadFromFile(path, &error_message));
    const auto rule = loaded.FindRule("VIP");
    HERMES_CHECK(static_cast<bool>(rule));
    HERMES_CHECK_EQ(rule->field, hermes::FilterField::kFrom);
    HERMES_CHECK_EQ(rule->operation, hermes::FilterOperation::kContains);
    HERMES_CHECK(static_cast<bool>(rule->destination_mailbox));
    HERMES_CHECK_EQ(*rule->destination_mailbox, std::string("Priority"));
    HERMES_CHECK(loaded.Remove("vip"));
}

HERMES_TEST(FilesystemFilterReportStoreFindsEntriesByMessageId) {
    hermes::tests::ScopedTempDirectory temp("hemera-filter-report");
    const auto path = temp.Path() / "filter-report.ini";

    hermes::FilesystemFilterReportStore store;
    store.AddEntry({"entry-1",
                    "message-1",
                    "inbox",
                    "Inbox",
                    "boss@example.com",
                    "Status",
                    {"boss", "priority"},
                    1710000000});
    std::string error_message;
    HERMES_CHECK(store.SaveToFile(path, &error_message));

    hermes::FilesystemFilterReportStore loaded;
    HERMES_CHECK(loaded.LoadFromFile(path, &error_message));
    const auto entry = loaded.FindByMessageId("message-1");
    HERMES_CHECK(static_cast<bool>(entry));
    HERMES_CHECK_EQ(entry->matched_rules.size(), static_cast<std::size_t>(2));
}

HERMES_TEST(FilesystemLinkHistoryStoreRoundTripsEntries) {
    hermes::tests::ScopedTempDirectory temp("hemera-link-history");
    const auto path = temp.Path() / "link-history.ini";

    hermes::FilesystemLinkHistoryStore store;
    store.AddEntry({"entry-1",
                    hermes::LinkHistoryKind::kAttachment,
                    "Quarterly Report",
                    "/tmp/report.pdf",
                    "message:inbox:123",
                    true,
                    1710000042});
    std::string error_message;
    HERMES_CHECK(store.SaveToFile(path, &error_message));

    hermes::FilesystemLinkHistoryStore loaded;
    HERMES_CHECK(loaded.LoadFromFile(path, &error_message));
    const auto entries = loaded.Entries();
    HERMES_CHECK_EQ(entries.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(entries.front().kind, hermes::LinkHistoryKind::kAttachment);
    HERMES_CHECK_EQ(entries.front().title, std::string("Quarterly Report"));
    const auto found = loaded.FindById("entry-1");
    HERMES_CHECK(static_cast<bool>(found));
    HERMES_CHECK_EQ(found->target, std::string("/tmp/report.pdf"));
    HERMES_CHECK(loaded.Remove("entry-1"));
    HERMES_CHECK(!loaded.FindById("entry-1"));
    HERMES_CHECK_EQ(loaded.Entries().size(), static_cast<std::size_t>(0));
}

HERMES_TEST(LocalDirectoryServiceCatalogSearchesNicknamesAndAddressBook) {
    hermes::FlatFileNicknameStore nicknames;
    nicknames.AddOrReplace({"dev-team",
                            "Development Team",
                            {"alice@example.com", "bob@example.com"},
                            "Daily triage list",
                            true,
                            false});

    hermes::MemoryAddressBookService address_book;
    address_book.AddEntry({"Cara Example", "cara@example.com", "Support"});

    hermes::LocalDirectoryServiceCatalog catalog(&nicknames, &address_book);
    const auto providers = catalog.Providers();
    HERMES_CHECK_EQ(providers.size(), static_cast<std::size_t>(2));

    const auto results = catalog.Search("cara");
    HERMES_CHECK_EQ(results.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(results.front().provider_id, std::string("address-book"));

    const auto nickname_results = catalog.Search("development");
    HERMES_CHECK_EQ(nickname_results.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(nickname_results.front().provider_id, std::string("nicknames"));
    HERMES_CHECK_EQ(nickname_results.front().email_addresses.size(), static_cast<std::size_t>(2));

    const auto provider_results = catalog.SearchProvider("nicknames", "development");
    HERMES_CHECK_EQ(provider_results.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(provider_results.front().provider_id, std::string("nicknames"));
    const auto provider_set_results =
        catalog.SearchProviders({"nicknames", "address-book"}, "example");
    HERMES_CHECK_EQ(provider_set_results.size(), static_cast<std::size_t>(2));
    HERMES_CHECK(provider_set_results[0].provider_id == std::string("nicknames") ||
                 provider_set_results[1].provider_id == std::string("nicknames"));
    HERMES_CHECK(provider_set_results[0].provider_id == std::string("address-book") ||
                 provider_set_results[1].provider_id == std::string("address-book"));

    const std::string detail = catalog.LongDetailText(provider_results.front());
    HERMES_CHECK(detail.find("Provider: Nicknames") != std::string::npos);
    HERMES_CHECK(detail.find("Addresses:") != std::string::npos);
    const std::string printable_detail = catalog.PrintableDetailText(provider_results.front());
    HERMES_CHECK(printable_detail.find("Directory Result") != std::string::npos);
    HERMES_CHECK(printable_detail.find("Development Team") != std::string::npos);
    const std::string aggregate_detail = catalog.LongDetailText(provider_results);
    HERMES_CHECK(aggregate_detail.find("Development Team") != std::string::npos);
    HERMES_CHECK(aggregate_detail.find("alice@example.com") != std::string::npos);
    HERMES_CHECK(aggregate_detail.find("bob@example.com") != std::string::npos);

    const std::string printable = catalog.PrintableText("nicknames", "development", provider_results);
    HERMES_CHECK(printable.find("Query: development") != std::string::npos);
    HERMES_CHECK(printable.find("Development Team") != std::string::npos);
    const std::string provider_set_detail =
        catalog.LongDetailText(std::vector<std::string>{"nicknames", "address-book"},
                               "development",
                               provider_results);
    HERMES_CHECK(provider_set_detail.find("Directory Providers: Nicknames, Address Book") != std::string::npos);
    const std::string provider_set_printable =
        catalog.PrintableText(std::vector<std::string>{"nicknames", "address-book"},
                              "development",
                              provider_results);
    HERMES_CHECK(provider_set_printable.find("Directory Providers: Nicknames, Address Book") !=
                 std::string::npos);
    const std::string provider_set_empty_detail =
        catalog.LongDetailText(std::vector<std::string>{"nicknames", "address-book"},
                               "",
                               std::vector<hermes::DirectoryEntry>{});
    HERMES_CHECK(provider_set_empty_detail.find("Directory Providers: Nicknames, Address Book") !=
                 std::string::npos);
    HERMES_CHECK(provider_set_empty_detail.find("Results: 0") != std::string::npos);
    const std::string provider_set_empty_printable =
        catalog.PrintableText(std::vector<std::string>{"nicknames", "address-book"},
                              "",
                              std::vector<hermes::DirectoryEntry>{});
    HERMES_CHECK(provider_set_empty_printable.find("Directory Providers: Nicknames, Address Book") !=
                 std::string::npos);

    HERMES_CHECK_EQ(hermes::DirectoryComposeAddress(provider_results.front()),
                    std::string("Development Team <alice@example.com>, Development Team <bob@example.com>"));
    HERMES_CHECK_EQ(catalog.ComposeAddressText(provider_results),
                    std::string("Development Team <alice@example.com>, Development Team <bob@example.com>"));
}

HERMES_TEST(FilesystemStationeryStoreSupportsCrudOperations) {
    hermes::tests::ScopedTempDirectory temp("hemera-stationery");

    hermes::FilesystemStationeryStore store;
    std::string error_message;
    HERMES_CHECK(store.Discover(temp.Path(), &error_message));

    hermes::StationeryTemplate template_value;
    template_value.name = "Support";
    template_value.headers.subject = "Support reply";
    template_value.persona = "support";
    template_value.signature_name = "Standard";
    template_value.body.plain_text = "Thanks for reaching out.";
    HERMES_CHECK(store.SaveTemplate(template_value, &error_message));

    const auto loaded = store.Find("support");
    HERMES_CHECK(static_cast<bool>(loaded));
    HERMES_CHECK_EQ(loaded->headers.subject, std::string("Support reply"));
    HERMES_CHECK_EQ(loaded->persona, std::string("support"));
    HERMES_CHECK(store.DeleteTemplate("Support", &error_message));
    HERMES_CHECK(!store.Find("Support"));
}

HERMES_TEST(InMemoryMailTaskModelSupportsRemovingAndClearingErrors) {
    hermes::InMemoryMailTaskModel model;
    model.UpsertTask({"send-1", "primary", "Send Mail", "Queued", "Queued for delivery"});
    HERMES_CHECK(model.FailTask("send-1",
                                "Failed",
                                "Authentication failed",
                                hermes::MailTaskErrorKind::kCredentialRejected,
                                "PLAIN"));
    HERMES_CHECK(model.FailTask("send-1",
                                "Failed",
                                "TLS negotiation failed",
                                hermes::MailTaskErrorKind::kHandshakeFailed,
                                "STARTTLS"));

    HERMES_CHECK_EQ(model.Errors().size(), static_cast<std::size_t>(2));
    HERMES_CHECK(model.RemoveError(0));
    const auto remaining_errors = model.Errors();
    HERMES_CHECK_EQ(remaining_errors.size(), static_cast<std::size_t>(1));
    HERMES_CHECK(!remaining_errors.front().message.empty());

    model.ClearErrors();
    HERMES_CHECK_EQ(model.Errors().size(), static_cast<std::size_t>(0));
}
