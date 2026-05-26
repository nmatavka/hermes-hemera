#include "hermes/MoodWatchAnalyzer.h"

#include <utility>

extern "C" {
#include "tae/TAE.h"
}

namespace hermes {

class TaeMoodWatchAnalyzer::Impl {
public:
    explicit Impl(std::filesystem::path dictionary_path) : dictionary_path_(std::move(dictionary_path)) {}

    ~Impl() {
        if (dictionary_loaded_) {
            TAECloseDictionary(&dictionary_state_);
        }
    }

    bool IsAvailable() const {
        EnsureDictionaryLoaded();
        return dictionary_loaded_;
    }

    MoodWatchResult Analyze(std::string_view text, const MoodWatchOptions& options) {
        MoodWatchResult result;
        EnsureDictionaryLoaded();
        result.available = dictionary_loaded_;
        if (!dictionary_loaded_) {
            return result;
        }

        TAESessionState session_state{};
        if (!TAEStartSession(&session_state, &dictionary_state_)) {
            return result;
        }

        std::string text_copy(text);
        std::vector<std::string> whitelist_storage = options.whitelist_words;
        std::vector<char*> whitelist;
        whitelist.reserve(whitelist_storage.size() + 1);
        for (auto& word : whitelist_storage) {
            whitelist.push_back(word.data());
        }
        whitelist.push_back(nullptr);

        TAEAllMatches all_matches{};
        TAEInitAllMatches(&all_matches);

        unsigned long flags = 0;
        if (options.contains_html) {
            flags |= TAE_CONTAINSHTML;
        }
        if (options.ignore_safe_text) {
            flags |= TAE_IGNORESAFETEXT;
        }

        if (TAEProcessText(&session_state,
                           text_copy.data(),
                           static_cast<long>(text_copy.size()),
                           &all_matches,
                           flags,
                           whitelist.data())) {
            TAEScoreData score_data{};
            TAEInitScoreData(&score_data);
            result.score = TAEGetScoreData(&session_state, &score_data, nullptr, 0);
            TAEFreeScoreData(&score_data);

            for (int index = 0; index < all_matches.iNumMatches; ++index) {
                const TAEMatch& match = all_matches.ptaematches[index];
                result.matches.push_back({static_cast<std::size_t>(match.lStart),
                                          static_cast<std::size_t>(match.lLength),
                                          match.nCollection});
            }
        }

        TAEFreeAllMatches(&all_matches);
        TAEEndSession(&session_state);
        return result;
    }

    std::filesystem::path DictionaryPath() const {
        return dictionary_path_;
    }

private:
    void EnsureDictionaryLoaded() const {
        if (load_attempted_) {
            return;
        }
        load_attempted_ = true;

        if (!std::filesystem::exists(dictionary_path_)) {
            dictionary_loaded_ = false;
            return;
        }

        std::string path = dictionary_path_.string();
        dictionary_loaded_ = TAEInitDictionary(&dictionary_state_, path.data()) != 0;
    }

    std::filesystem::path dictionary_path_;
    mutable TAEDictState dictionary_state_{};
    mutable bool load_attempted_ = false;
    mutable bool dictionary_loaded_ = false;
};

TaeMoodWatchAnalyzer::TaeMoodWatchAnalyzer(std::filesystem::path dictionary_path)
    : impl_(std::make_unique<Impl>(std::move(dictionary_path))) {}

TaeMoodWatchAnalyzer::~TaeMoodWatchAnalyzer() = default;

bool TaeMoodWatchAnalyzer::IsAvailable() const {
    return impl_->IsAvailable();
}

MoodWatchResult TaeMoodWatchAnalyzer::Analyze(std::string_view text, const MoodWatchOptions& options) {
    return impl_->Analyze(text, options);
}

std::filesystem::path TaeMoodWatchAnalyzer::DictionaryPath() const {
    return impl_->DictionaryPath();
}

}  // namespace hermes
