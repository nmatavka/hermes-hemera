#include <fstream>
#include <iterator>

#include "TestRegistry.h"

#include "hermes/MoodWatchAnalyzer.h"

namespace {

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

HERMES_TEST(TaeMoodWatchAnalyzerScoresLegacyFixturesWithTrackedDictionary) {
    hermes::TaeMoodWatchAnalyzer analyzer(
        std::filesystem::path(HERMES_SOURCE_ROOT) / "src" / "legacy_transplants" / "tae" / "FlameLex.dat");

    HERMES_CHECK(analyzer.IsAvailable());

    const std::string no_flame =
        ReadTextFile(std::filesystem::path(HERMES_FIXTURE_ROOT) / "compose" / "mood" / "no-flame.txt");
    const std::string medium_flame =
        ReadTextFile(std::filesystem::path(HERMES_FIXTURE_ROOT) / "compose" / "mood" / "med-flame.txt");

    const auto calm = analyzer.Analyze(no_flame);
    const auto heated = analyzer.Analyze(medium_flame);

    HERMES_CHECK(calm.available);
    HERMES_CHECK(heated.available);
    HERMES_CHECK(heated.score > calm.score);
    HERMES_CHECK(!heated.matches.empty());
}
