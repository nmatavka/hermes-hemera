#include <algorithm>

#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/HelpCatalog.h"

HERMES_TEST(LegacyHelpCatalogLoadsTrackedHelpTopicMap) {
    hermes::LegacyHelpCatalog catalog;
    std::string error_message;
    HERMES_CHECK(catalog.LoadTopicMap(
        hermes::tests::FixtureRoot() / "help" / "eudora.hh",
        &error_message));

    HERMES_CHECK(catalog.Topics().size() > 100);
    const auto topic = catalog.FindById("HIDD_SETTINGS_SPELL");
    HERMES_CHECK(static_cast<bool>(topic));
    HERMES_CHECK_EQ(topic->id, std::string("HIDD_SETTINGS_SPELL"));
}

HERMES_TEST(LegacyHelpCatalogParsesDefineStyleTopicMaps) {
    hermes::LegacyHelpCatalog catalog;
    std::string error_message;
    HERMES_CHECK(catalog.LoadTopicMap(
        hermes::tests::FixtureRoot() / "help" / "Options.hh",
        &error_message));

    const auto topic = catalog.FindById("Include_signature_on_reply");
    HERMES_CHECK(static_cast<bool>(topic));
}

HERMES_TEST(LegacyHelpCatalogParsesContentsHierarchy) {
    hermes::LegacyHelpCatalog catalog;
    std::string error_message;
    HERMES_CHECK(catalog.LoadContents(
        hermes::tests::FixtureRoot() / "help" / "Eudora.cnt",
        &error_message));

    const auto contents = catalog.Contents();
    HERMES_CHECK(contents.size() > 50);
    const auto it = std::find_if(contents.begin(), contents.end(), [](const hermes::HelpContentsEntry& entry) {
        return entry.label == "Configuring Eudora";
    });
    HERMES_CHECK(it != contents.end());
    HERMES_CHECK_EQ(it->level, 3);
    HERMES_CHECK_EQ(it->topic_id, std::string("Configuring_Eudora"));
}
