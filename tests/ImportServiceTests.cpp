#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/ImportService.h"

HERMES_TEST(LegacyImportServiceDiscoversTrackedLegacyArtifacts) {
    hermes::LegacyImportService service;
    const auto artifacts = service.Discover(hermes::tests::FixtureRoot());
    HERMES_CHECK(!artifacts.empty());

    bool found_settings_snapshot = false;
    bool found_help_map = false;
    bool found_import_config = false;
    for (const auto& artifact : artifacts) {
        found_settings_snapshot =
            found_settings_snapshot || artifact.kind == hermes::ImportArtifactKind::kSettingsSnapshot;
        found_help_map = found_help_map || artifact.kind == hermes::ImportArtifactKind::kHelpTopicMap;
        found_import_config = found_import_config || artifact.kind == hermes::ImportArtifactKind::kImportConfig;
    }

    HERMES_CHECK(found_settings_snapshot);
    HERMES_CHECK(found_help_map);
    HERMES_CHECK(found_import_config);
}

HERMES_TEST(LegacyImportServiceCopiesArtifactsIntoTrackedLayout) {
    hermes::LegacyImportService service;
    hermes::tests::ScopedTempDirectory temp("hemera-import");

    std::string error_message;
    HERMES_CHECK(service.Import(hermes::tests::FixtureRoot(), temp.Path(), &error_message));

    HERMES_CHECK(std::filesystem::exists(temp.Path() / "profile_snapshots" / "Eudora.box"));
    HERMES_CHECK(std::filesystem::exists(temp.Path() / "help" / "eudora.hh"));
    HERMES_CHECK(std::filesystem::exists(temp.Path() / "import" / "WBImport.INI"));
}
