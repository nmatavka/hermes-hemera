#include <filesystem>
#include <fstream>

#include "TestRegistry.h"

#include "hermes/IniSettingsStore.h"

HERMES_TEST(IniSettingsStoreLoadsLegacyStyleValues) {
    const std::filesystem::path temp_path =
        std::filesystem::temp_directory_path() / "hermes-hemera-settings.ini";

    std::ofstream output(temp_path);
    output << "[Settings]\n";
    output << "UseTrident=0\n";
    output << "FontSize=14\n";
    output.close();

    hermes::IniSettingsStore store;
    std::string error_message;
    HERMES_CHECK(store.LoadFromFile(temp_path, &error_message));
    HERMES_CHECK(store.GetBool("Settings", "UseTrident", true) == false);
    HERMES_CHECK_EQ(store.GetInt("Settings", "FontSize", 0), 14);
}

HERMES_TEST(IniSettingsStoreSavesRoundTripValues) {
    const std::filesystem::path temp_path =
        std::filesystem::temp_directory_path() / "hermes-hemera-roundtrip.ini";

    hermes::IniSettingsStore store;
    store.SetString("Filters", "Enabled", "1");
    store.SetString("Persona-1", "ReturnAddress", "haiku@example.com");

    std::string error_message;
    HERMES_CHECK(store.SaveToFile(temp_path, &error_message));

    hermes::IniSettingsStore reloaded;
    HERMES_CHECK(reloaded.LoadFromFile(temp_path, &error_message));
    HERMES_CHECK(reloaded.GetBool("Filters", "Enabled", false));
    HERMES_CHECK_EQ(
        reloaded.GetString("Persona-1", "ReturnAddress").value_or(""),
        std::string("haiku@example.com"));
}
