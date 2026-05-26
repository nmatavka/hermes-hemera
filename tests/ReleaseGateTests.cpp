#include "TestPaths.h"
#include "TestRegistry.h"

HERMES_TEST(TrackedLegacyFixturesPopulateRequiredLandingZones) {
    const auto fixture_root = hermes::tests::FixtureRoot();
    HERMES_CHECK(std::filesystem::exists(fixture_root / "profile_snapshots" / "Eudora.box"));
    HERMES_CHECK(std::filesystem::exists(fixture_root / "help" / "eudora.hh"));
    HERMES_CHECK(std::filesystem::exists(fixture_root / "import" / "WBImport.INI"));
}

HERMES_TEST(TrackedPortingDocsIncludeSslAndHelpSources) {
    HERMES_CHECK(std::filesystem::exists(
        std::filesystem::path(HERMES_SOURCE_ROOT) / "docs" / "porting" / "ssl" / "SSL Notes.txt"));
    HERMES_CHECK(std::filesystem::exists(
        std::filesystem::path(HERMES_SOURCE_ROOT) / "docs" / "porting" / "help" / "eudora.hh"));
}
