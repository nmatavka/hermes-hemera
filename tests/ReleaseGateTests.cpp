#include "TestPaths.h"
#include "TestRegistry.h"

#include <fstream>

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

HERMES_TEST(TrackedGitignoreKeepsAuditOnlyLegacyTreesOutOfRecord) {
    const auto gitignore_path = std::filesystem::path(HERMES_SOURCE_ROOT) / ".gitignore";
    std::ifstream input(gitignore_path);
    HERMES_CHECK(input.good());

    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    HERMES_CHECK(contents.find("/legacy/eudora-drop/") != std::string::npos);
    HERMES_CHECK(contents.find("/legacy/V624/") != std::string::npos);
}

HERMES_TEST(TrackedShellFidelityMatrixCapturesWindowsWorkflowAudit) {
    const auto matrix_path = hermes::tests::FixtureRoot() / "shell_fidelity_matrix.txt";
    HERMES_CHECK(std::filesystem::exists(matrix_path));

    std::ifstream input(matrix_path);
    HERMES_CHECK(input.good());
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    HERMES_CHECK(contents.find("TocFrame") != std::string::npos);
    HERMES_CHECK(contents.find("CompMessageFrame") != std::string::npos);
    HERMES_CHECK(contents.find("TaskStatusView") != std::string::npos);
    HERMES_CHECK(contents.find("NicknamesWazooWnd") != std::string::npos);
    HERMES_CHECK(contents.find("HaikuMainWindow") != std::string::npos);
    HERMES_CHECK(contents.find("HaikuComposeWindow") != std::string::npos);
    HERMES_CHECK(contents.find("Junk-column presentation") != std::string::npos);
    HERMES_CHECK(contents.find("explicit tree command routing") != std::string::npos);
    HERMES_CHECK(contents.find("CompMessageFrame / PgCompMsgView | HaikuComposeWindow | in_progress") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("DirectoryServicesWazooWndNew | HaikuWazooWindow(directory-services) | in_progress") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("MboxWazooWnd / mboxtree.cpp / DynamicMailboxMenu.cpp | HaikuWazooWindow(mailboxes) / HaikuMainWindow | implemented") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("QCToolBarManager | HaikuToolbarSupport / toolbar customization dialog | implemented") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("QCFindMgr / mainfrm.cpp / SearchBar.cpp | HaikuMainWindow / HaikuWazooWindow / HaikuComposeWindow / shared shell find routing | implemented") !=
                 std::string::npos);
    HERMES_CHECK(contents.find("V624") == std::string::npos);
}

HERMES_TEST(TrackedHaikuReleaseScaffoldingExistsForHemeraRc) {
    const auto source_root = std::filesystem::path(HERMES_SOURCE_ROOT);
    const auto rdef_path = source_root / "packaging" / "haiku" / "Hemera.rdef.in";
    const auto package_info_path = source_root / "packaging" / "haiku" / "Hemera.PackageInfo.in";
    const auto packaging_readme_path = source_root / "packaging" / "haiku" / "README.md";
    const auto recipe_path =
        source_root / "packaging" / "haikuports" / "hemera" / "hemera-1.0.0~rc1.recipe.in";
    const auto notice_path =
        source_root / "src" / "hermes_port" / "haiku" / "assets" / "icons" / "NOTICE.md";
    const auto release_notes_path = source_root / "docs" / "release-notes" / "1.0.0-rc1.md";
    const auto app_icon_path =
        source_root / "src" / "hermes_port" / "haiku" / "assets" / "icons" / "app" / "hemera-mail.hvif";
    const auto toolbar_icon_path =
        source_root / "src" / "hermes_port" / "haiku" / "assets" / "icons" / "toolbar" / "mail-send.hvif";

    HERMES_CHECK(std::filesystem::exists(rdef_path));
    HERMES_CHECK(std::filesystem::exists(package_info_path));
    HERMES_CHECK(std::filesystem::exists(packaging_readme_path));
    HERMES_CHECK(std::filesystem::exists(recipe_path));
    HERMES_CHECK(std::filesystem::exists(notice_path));
    HERMES_CHECK(std::filesystem::exists(release_notes_path));
    HERMES_CHECK(std::filesystem::exists(app_icon_path));
    HERMES_CHECK(std::filesystem::exists(toolbar_icon_path));

    std::ifstream rdef_input(rdef_path);
    std::ifstream package_input(package_info_path);
    std::ifstream packaging_readme_input(packaging_readme_path);
    std::ifstream recipe_input(recipe_path);
    std::ifstream notice_input(notice_path);
    std::ifstream release_notes_input(release_notes_path);
    HERMES_CHECK(rdef_input.good());
    HERMES_CHECK(package_input.good());
    HERMES_CHECK(packaging_readme_input.good());
    HERMES_CHECK(recipe_input.good());
    HERMES_CHECK(notice_input.good());
    HERMES_CHECK(release_notes_input.good());

    const std::string rdef_contents((std::istreambuf_iterator<char>(rdef_input)),
                                    std::istreambuf_iterator<char>());
    const std::string package_contents((std::istreambuf_iterator<char>(package_input)),
                                       std::istreambuf_iterator<char>());
    const std::string packaging_readme_contents((std::istreambuf_iterator<char>(packaging_readme_input)),
                                                std::istreambuf_iterator<char>());
    const std::string recipe_contents((std::istreambuf_iterator<char>(recipe_input)),
                                      std::istreambuf_iterator<char>());
    const std::string notice_contents((std::istreambuf_iterator<char>(notice_input)),
                                      std::istreambuf_iterator<char>());
    const std::string release_notes_contents((std::istreambuf_iterator<char>(release_notes_input)),
                                             std::istreambuf_iterator<char>());

    HERMES_CHECK(rdef_contents.find("resource app_signature \"@HEMERA_HAIKU_APP_SIGNATURE@\";") !=
                 std::string::npos);
    HERMES_CHECK(rdef_contents.find("short_info = \"Hemera\"") != std::string::npos);
    HERMES_CHECK(rdef_contents.find("long_info = \"Hemera @HEMERA_RC_VERSION@\"") !=
                 std::string::npos);
    HERMES_CHECK(package_contents.find("name                  hemera") != std::string::npos);
    HERMES_CHECK(package_contents.find("summary               \"Hemera mail client\"") != std::string::npos);
    HERMES_CHECK(package_contents.find("vendor                \"HERMES\"") != std::string::npos);
    HERMES_CHECK(package_contents.find("app:Hemera = @HEMERA_HAIKU_PACKAGE_VERSION@") !=
                 std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("Hemera") != std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("application/x-vnd.hermes-hemera") !=
                 std::string::npos);
    HERMES_CHECK(recipe_contents.find("SUMMARY=\"Hemera mail client\"") != std::string::npos);
    HERMES_CHECK(recipe_contents.find("cmake --build build --target Hemera") != std::string::npos);
    HERMES_CHECK(notice_contents.find("Hemera") != std::string::npos);
    HERMES_CHECK(notice_contents.find("retrosmart-icon-theme") != std::string::npos);
    HERMES_CHECK(release_notes_contents.find("# Hemera 1.0.0 RC1") != std::string::npos);
    HERMES_CHECK(release_notes_contents.find("compose final dispatch validation") != std::string::npos);
    HERMES_CHECK(release_notes_contents.find("Directory Services preview-close/reset validation") !=
                 std::string::npos);
}
