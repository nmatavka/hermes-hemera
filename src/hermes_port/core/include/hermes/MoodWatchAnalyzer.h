#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

struct MoodWatchOptions {
    std::vector<std::string> whitelist_words;
    bool contains_html = false;
    bool ignore_safe_text = true;
};

struct MoodWatchMatch {
    std::size_t offset = 0;
    std::size_t length = 0;
    short collection_id = 0;
};

struct MoodWatchResult {
    bool available = false;
    int score = -1;
    std::vector<MoodWatchMatch> matches;
};

class MoodWatchAnalyzer {
public:
    virtual ~MoodWatchAnalyzer() = default;

    virtual bool IsAvailable() const = 0;
    virtual MoodWatchResult Analyze(std::string_view text, const MoodWatchOptions& options = {}) = 0;
};

class TaeMoodWatchAnalyzer final : public MoodWatchAnalyzer {
public:
    explicit TaeMoodWatchAnalyzer(std::filesystem::path dictionary_path);
    ~TaeMoodWatchAnalyzer() override;

    TaeMoodWatchAnalyzer(const TaeMoodWatchAnalyzer&) = delete;
    TaeMoodWatchAnalyzer& operator=(const TaeMoodWatchAnalyzer&) = delete;

    bool IsAvailable() const override;
    MoodWatchResult Analyze(std::string_view text, const MoodWatchOptions& options = {}) override;

    std::filesystem::path DictionaryPath() const;

private:
    class Impl;

    std::unique_ptr<Impl> impl_;
};

}  // namespace hermes
