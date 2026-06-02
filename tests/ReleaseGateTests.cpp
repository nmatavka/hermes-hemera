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

HERMES_TEST(TrackedHaikuReadmeDocumentsAutoFetchAndOptionalBootstrap) {
    const auto readme_path = std::filesystem::path(HERMES_SOURCE_ROOT) / "README-HAIKU-PORT.md";
    std::ifstream input(readme_path);
    HERMES_CHECK(input.good());

    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    HERMES_CHECK(contents.find("build/_deps/") != std::string::npos);
    HERMES_CHECK(contents.find("optional prewarm/offline helper") != std::string::npos);
    HERMES_CHECK(contents.find("Run [`scripts/bootstrap_dependencies.sh`]") == std::string::npos);
    HERMES_CHECK(contents.find("cmake -S . -B build") != std::string::npos);
    HERMES_CHECK(contents.find("release_haiku_rollout.sh") != std::string::npos);
    HERMES_CHECK(contents.find("packaging/haikuports/mail-client/hemera/hemera.recipe.in") !=
                 std::string::npos);
}

HERMES_TEST(TrackedDependencyBootstrapUsesSharedManifestAndPaigePatchset) {
    const auto source_root = std::filesystem::path(HERMES_SOURCE_ROOT);
    const auto script_path = source_root / "scripts" / "bootstrap_dependencies.sh";
    const auto manifest_path = source_root / "cmake" / "HermesDependencyRefs.env";
    const auto dependency_cmake_path = source_root / "cmake" / "HermesDependencies.cmake";
    const auto paige_patch_path =
        source_root / "cmake" / "patches" / "hermes-paige" / "0001-haiku-include-pgdefstl.patch";
    std::ifstream script_input(script_path);
    std::ifstream manifest_input(manifest_path);
    std::ifstream dependency_cmake_input(dependency_cmake_path);
    std::ifstream paige_patch_input(paige_patch_path);
    HERMES_CHECK(script_input.good());
    HERMES_CHECK(manifest_input.good());
    HERMES_CHECK(dependency_cmake_input.good());
    HERMES_CHECK(paige_patch_input.good());

    const std::string script_contents((std::istreambuf_iterator<char>(script_input)),
                                      std::istreambuf_iterator<char>());
    const std::string manifest_contents((std::istreambuf_iterator<char>(manifest_input)),
                                        std::istreambuf_iterator<char>());
    const std::string dependency_cmake_contents((std::istreambuf_iterator<char>(dependency_cmake_input)),
                                                std::istreambuf_iterator<char>());
    const std::string paige_patch_contents((std::istreambuf_iterator<char>(paige_patch_input)),
                                           std::istreambuf_iterator<char>());

    HERMES_CHECK(script_contents.find("source \"${DEPENDENCY_REFS_FILE}\"") != std::string::npos);
    HERMES_CHECK(script_contents.find("apply_patchset") != std::string::npos);
    HERMES_CHECK(script_contents.find("HEMERA_DEP_KRB5_REF") != std::string::npos);
    HERMES_CHECK(manifest_contents.find("HEMERA_DEP_HERMES_PAIGE_REF=cc543e957afb61d054862021f589dc75f2c8406c") !=
                 std::string::npos);
    HERMES_CHECK(manifest_contents.find("HEMERA_DEP_OPENSSL_REF=openssl-4.0.0") !=
                 std::string::npos);
    HERMES_CHECK(manifest_contents.find("HEMERA_DEP_HUNSPELL_REF=v1.7.3") !=
                 std::string::npos);
    HERMES_CHECK(manifest_contents.find("HEMERA_DEP_KRB5_REF=krb5-1.22.2-final") !=
                 std::string::npos);
    HERMES_CHECK(manifest_contents.find("367c01f1510304f90c7c944e2e356bebe8eef040") == std::string::npos);
    HERMES_CHECK(dependency_cmake_contents.find("HERMES_DEPENDENCY_REF_FILE") != std::string::npos);
    HERMES_CHECK(dependency_cmake_contents.find("hemera_apply_patchset") != std::string::npos);
    HERMES_CHECK(paige_patch_contents.find("PGDEFSTL.H") != std::string::npos);
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
    const auto release_manifest_path = source_root / "packaging" / "haiku" / "release_manifest.yml";
    const auto packaging_readme_path = source_root / "packaging" / "haiku" / "README.md";
    const auto field_order_path = source_root / "packaging" / "haiku" / "haikuporter_field_order.txt";
    const auto recipe_path = source_root / "packaging" / "haikuports" / "mail-client" / "hemera" /
                             "hemera.recipe.in";
    const auto rollout_tool_dir = source_root / "tools" / "haiku_rollout";
    const auto rollout_readme_path = rollout_tool_dir / "README.md";
    const auto rollout_mix_path = rollout_tool_dir / "mix.exs";
    const auto rollout_launcher_path = source_root / "scripts" / "release_haiku_rollout.sh";
    const auto stale_tempscripts_path = source_root / "scripts" / "tempscripts";
    const auto notice_path =
        source_root / "src" / "hermes_port" / "haiku" / "assets" / "icons" / "NOTICE.md";
    const auto release_notes_path = source_root / "docs" / "release-notes" / "1.0.md";
    const auto app_icon_path =
        source_root / "src" / "hermes_port" / "haiku" / "assets" / "icons" / "app" / "hemera-mail.hvif";
    const auto toolbar_icon_path =
        source_root / "src" / "hermes_port" / "haiku" / "assets" / "icons" / "toolbar" / "mail-send.hvif";

    HERMES_CHECK(std::filesystem::exists(rdef_path));
    HERMES_CHECK(std::filesystem::exists(package_info_path));
    HERMES_CHECK(std::filesystem::exists(release_manifest_path));
    HERMES_CHECK(std::filesystem::exists(packaging_readme_path));
    HERMES_CHECK(std::filesystem::exists(field_order_path));
    HERMES_CHECK(std::filesystem::exists(recipe_path));
    HERMES_CHECK(std::filesystem::exists(rollout_tool_dir));
    HERMES_CHECK(std::filesystem::exists(rollout_readme_path));
    HERMES_CHECK(std::filesystem::exists(rollout_mix_path));
    HERMES_CHECK(std::filesystem::exists(rollout_launcher_path));
    HERMES_CHECK(!std::filesystem::exists(stale_tempscripts_path));
    HERMES_CHECK(std::filesystem::exists(notice_path));
    HERMES_CHECK(std::filesystem::exists(release_notes_path));
    HERMES_CHECK(std::filesystem::exists(app_icon_path));
    HERMES_CHECK(std::filesystem::exists(toolbar_icon_path));

    std::ifstream rdef_input(rdef_path);
    std::ifstream package_input(package_info_path);
    std::ifstream release_manifest_input(release_manifest_path);
    std::ifstream packaging_readme_input(packaging_readme_path);
    std::ifstream field_order_input(field_order_path);
    std::ifstream recipe_input(recipe_path);
    std::ifstream rollout_readme_input(rollout_readme_path);
    std::ifstream rollout_mix_input(rollout_mix_path);
    std::ifstream rollout_launcher_input(rollout_launcher_path);
    std::ifstream notice_input(notice_path);
    std::ifstream release_notes_input(release_notes_path);
    HERMES_CHECK(rdef_input.good());
    HERMES_CHECK(package_input.good());
    HERMES_CHECK(release_manifest_input.good());
    HERMES_CHECK(packaging_readme_input.good());
    HERMES_CHECK(field_order_input.good());
    HERMES_CHECK(recipe_input.good());
    HERMES_CHECK(rollout_readme_input.good());
    HERMES_CHECK(rollout_mix_input.good());
    HERMES_CHECK(rollout_launcher_input.good());
    HERMES_CHECK(notice_input.good());
    HERMES_CHECK(release_notes_input.good());

    const std::string rdef_contents((std::istreambuf_iterator<char>(rdef_input)),
                                    std::istreambuf_iterator<char>());
    const std::string package_contents((std::istreambuf_iterator<char>(package_input)),
                                       std::istreambuf_iterator<char>());
    const std::string release_manifest_contents((std::istreambuf_iterator<char>(release_manifest_input)),
                                                std::istreambuf_iterator<char>());
    const std::string packaging_readme_contents((std::istreambuf_iterator<char>(packaging_readme_input)),
                                                std::istreambuf_iterator<char>());
    const std::string field_order_contents((std::istreambuf_iterator<char>(field_order_input)),
                                           std::istreambuf_iterator<char>());
    const std::string recipe_contents((std::istreambuf_iterator<char>(recipe_input)),
                                      std::istreambuf_iterator<char>());
    const std::string rollout_readme_contents((std::istreambuf_iterator<char>(rollout_readme_input)),
                                              std::istreambuf_iterator<char>());
    const std::string rollout_mix_contents((std::istreambuf_iterator<char>(rollout_mix_input)),
                                           std::istreambuf_iterator<char>());
    const std::string rollout_launcher_contents((std::istreambuf_iterator<char>(rollout_launcher_input)),
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
    HERMES_CHECK(package_contents.find("release candidate") == std::string::npos);
    HERMES_CHECK(release_manifest_contents.find("release_manifest") == std::string::npos);
    HERMES_CHECK(release_manifest_contents.find("version: \"1.0\"") != std::string::npos);
    HERMES_CHECK(release_manifest_contents.find("branch_template: \"hemera-<haikuports_version>\"") !=
                 std::string::npos);
    HERMES_CHECK(release_manifest_contents.find("pr_notes_template:") != std::string::npos);
    HERMES_CHECK(release_manifest_contents.find("pr_body_template:") == std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("Hemera") != std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("application/x-vnd.hermes-hemera") !=
                 std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("release_manifest.yml") != std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("HaikuPorter field ordering file") !=
                 std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("guided `gh pr create` handoff") !=
                 std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("managed fork branch has diverged remotely") !=
                 std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("does not inject PR body text") !=
                 std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("contributor checklist template") !=
                 std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("scripts/release_haiku_rollout.sh") !=
                 std::string::npos);
    HERMES_CHECK(packaging_readme_contents.find("resume 1.0") != std::string::npos);
    HERMES_CHECK(field_order_contents.find("SUMMARY") != std::string::npos);
    HERMES_CHECK(field_order_contents.find("INSTALL") != std::string::npos);
    HERMES_CHECK(recipe_contents.find("SUMMARY=\"Hemera mail client\"") != std::string::npos);
    HERMES_CHECK(recipe_contents.find("release candidate") == std::string::npos);
    HERMES_CHECK(recipe_contents.find("HOMEPAGE=\"https://github.com/@HEMERA_REPO_SLUG@\"") !=
                 std::string::npos);
    HERMES_CHECK(recipe_contents.find("cmake --build build --target Hemera") != std::string::npos);
    HERMES_CHECK(rollout_readme_contents.find("scripts/release_haiku_rollout.sh") !=
                 std::string::npos);
    HERMES_CHECK(rollout_readme_contents.find("HaikuPorts") != std::string::npos);
    HERMES_CHECK(rollout_readme_contents.find("release_manifest.yml") != std::string::npos);
    HERMES_CHECK(rollout_readme_contents.find("config status") != std::string::npos);
    HERMES_CHECK(rollout_readme_contents.find("resume <version>") != std::string::npos);
    HERMES_CHECK(rollout_readme_contents.find("state.json") != std::string::npos);
    HERMES_CHECK(rollout_readme_contents.find("exact `gh pr create` handoff command") !=
                 std::string::npos);
    HERMES_CHECK(rollout_readme_contents.find("--editor") != std::string::npos);
    HERMES_CHECK(rollout_readme_contents.find("does not inject PR body text") != std::string::npos);
    HERMES_CHECK(rollout_readme_contents.find("stops before push") != std::string::npos);
    HERMES_CHECK(rollout_mix_contents.find("app: :hemera_haiku_rollout") != std::string::npos);
    HERMES_CHECK(rollout_launcher_contents.find("tools/haiku_rollout") != std::string::npos);
    HERMES_CHECK(rollout_launcher_contents.find("mix deps.get") != std::string::npos);
    HERMES_CHECK(rollout_launcher_contents.find("--cwd \"${REPO_ROOT}\"") != std::string::npos);
    HERMES_CHECK(notice_contents.find("Hemera") != std::string::npos);
    HERMES_CHECK(notice_contents.find("retrosmart-icon-theme") != std::string::npos);
    HERMES_CHECK(release_notes_contents.find("# Hemera 1.0") != std::string::npos);
    HERMES_CHECK(release_notes_contents.find("compose final dispatch needs broader live Haiku runtime validation") != std::string::npos);
    HERMES_CHECK(release_notes_contents.find("Directory Services preview-close/reset needs broader live Haiku runtime validation") !=
                 std::string::npos);
}
