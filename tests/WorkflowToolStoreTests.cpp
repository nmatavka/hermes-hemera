#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/AddressBookService.h"
#include "hermes/DirectoryServiceCatalog.h"
#include "hermes/FilterReportStore.h"
#include "hermes/FilterStore.h"
#include "hermes/LinkHistoryStore.h"
#include "hermes/NicknameStore.h"
#include "hermes/StationeryStore.h"

HERMES_TEST(FilesystemFilterStoreRoundTripsRules) {
    hermes::tests::ScopedTempDirectory temp("hermes-filters");
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
    hermes::tests::ScopedTempDirectory temp("hermes-filter-report");
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
    hermes::tests::ScopedTempDirectory temp("hermes-link-history");
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
    HERMES_CHECK_EQ(nickname_results.size(), static_cast<std::size_t>(2));
    HERMES_CHECK_EQ(nickname_results.front().provider_id, std::string("nicknames"));
}

HERMES_TEST(FilesystemStationeryStoreSupportsCrudOperations) {
    hermes::tests::ScopedTempDirectory temp("hermes-stationery");

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
