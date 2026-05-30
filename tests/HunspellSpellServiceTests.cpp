#include "TestRegistry.h"

#include <algorithm>
#include <filesystem>

#include "hermes/HunspellSpellService.h"

namespace {

std::filesystem::path BundledHunspellAsset(std::string_view file_name) {
    return std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "hermes_port" / "haiku" / "assets" /
           "hunspell" / std::string(file_name);
}

}  // namespace

HERMES_TEST(BundledHunspellAssetsExistOutsideThirdParty) {
    HERMES_CHECK(std::filesystem::exists(BundledHunspellAsset("base_utf.aff")));
    HERMES_CHECK(std::filesystem::exists(BundledHunspellAsset("base_utf.dic")));
}

HERMES_TEST(HunspellSpellServiceUsesBundledSeedDictionaryWhenAvailable) {
    hermes::HunspellSpellService service(
        BundledHunspellAsset("base_utf.aff"),
        BundledHunspellAsset("base_utf.dic"));

#if HERMES_HAS_HUNSPELL
    HERMES_CHECK(service.IsAvailable());
    const auto issues = service.Check("hlelo hello", {});
    HERMES_CHECK_EQ(issues.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(issues.front().word, std::string("hlelo"));

    const auto suggestions = service.Suggest("hlelo", {});
    HERMES_CHECK(std::find(suggestions.begin(), suggestions.end(), std::string("hello")) != suggestions.end());
#else
    HERMES_CHECK(!service.IsAvailable());
#endif
}
