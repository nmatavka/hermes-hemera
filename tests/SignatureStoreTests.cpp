#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/SignatureStore.h"

HERMES_TEST(FilesystemSignatureStoreDiscoversPlainAndHtmlSignatures) {
    hermes::FilesystemSignatureStore store;
    std::string error_message;
    HERMES_CHECK(store.Discover(hermes::tests::FixtureRoot() / "compose" / "signatures", &error_message));

    const auto signatures = store.Templates();
    HERMES_CHECK_EQ(signatures.size(), static_cast<std::size_t>(2));

    const auto standard = store.Find("Standard");
    HERMES_CHECK(static_cast<bool>(standard));
    HERMES_CHECK(standard->body.plain_text.find("Nick Example") != std::string::npos);
    HERMES_CHECK(standard->body.html_fragment.empty());

    const auto alternate = store.Find("Alternate");
    HERMES_CHECK(static_cast<bool>(alternate));
    HERMES_CHECK(!alternate->body.html_fragment.empty());
    HERMES_CHECK(alternate->body.plain_text.find("Status Desk") != std::string::npos);
}
