#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/AddressBookService.h"
#include "hermes/NicknameStore.h"

HERMES_TEST(MemoryAddressBookServiceNormalizesEmailLookups) {
    hermes::MemoryAddressBookService address_book;
    address_book.AddEntry({"Alice Example", "Alice@Example.com", "friend"});

    HERMES_CHECK(address_book.Contains("alice@example.com"));
    HERMES_CHECK(address_book.Contains(" ALICE@EXAMPLE.COM "));
    HERMES_CHECK_EQ(address_book.Entries().size(), static_cast<std::size_t>(1));
}

HERMES_TEST(FlatFileNicknameStoreRoundTripsEntries) {
    hermes::tests::ScopedTempDirectory temp("hermes-nicknames");
    const std::filesystem::path path = temp.Path() / "nicknames.txt";

    hermes::FlatFileNicknameStore store;
    store.AddOrReplace({"dev-team",
                        "Development Team",
                        {"alice@example.com", "bob@example.com"},
                        "Daily triage list",
                        true,
                        false});

    std::string error_message;
    HERMES_CHECK(store.SaveToFile(path, &error_message));

    hermes::FlatFileNicknameStore loaded;
    HERMES_CHECK(loaded.LoadFromFile(path, &error_message));

    const auto entry = loaded.FindNickname("DEV-TEAM");
    HERMES_CHECK(static_cast<bool>(entry));
    HERMES_CHECK_EQ(entry->full_name, std::string("Development Team"));
    HERMES_CHECK_EQ(entry->addresses.size(), static_cast<std::size_t>(2));
    HERMES_CHECK(entry->recipient_list);
}

HERMES_TEST(FlatFileNicknameStoreSupportsRemoval) {
    hermes::FlatFileNicknameStore store;
    store.AddOrReplace({"dev-team", "Development Team", {"alice@example.com"}, "", false, false});
    store.AddOrReplace({"qa-team", "QA Team", {"qa@example.com"}, "", false, false});

    HERMES_CHECK(store.Remove("DEV-TEAM"));
    HERMES_CHECK(!store.FindNickname("dev-team"));
    HERMES_CHECK_EQ(store.Entries().size(), static_cast<std::size_t>(1));
}
