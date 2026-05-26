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
