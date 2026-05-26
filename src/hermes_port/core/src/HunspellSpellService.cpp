#include "hermes/HunspellSpellService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_set>

#if HERMES_HAS_HUNSPELL
#if __has_include(<hunspell/hunspell.h>)
extern "C" {
#include <hunspell/hunspell.h>
}
#define HERMES_INTERNAL_HAS_HUNSPELL 1
#elif __has_include(<hunspell.h>)
extern "C" {
#include <hunspell.h>
}
#define HERMES_INTERNAL_HAS_HUNSPELL 1
#else
#define HERMES_INTERNAL_HAS_HUNSPELL 0
#endif
#else
#define HERMES_INTERNAL_HAS_HUNSPELL 0
#endif

namespace hermes {

struct HunspellSpellService::Impl {
    std::filesystem::path aff_path;
    std::filesystem::path dic_path;
    std::unordered_set<std::string> ignored_words;
#if HERMES_INTERNAL_HAS_HUNSPELL
    Hunhandle* handle = nullptr;
#endif
};

namespace {

std::string ToLower(std::string_view value) {
    std::string lowered(value);
    std::transform(
        lowered.begin(),
        lowered.end(),
        lowered.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
}

}  // namespace

HunspellSpellService::HunspellSpellService(std::filesystem::path aff_path, std::filesystem::path dic_path)
    : impl_(std::make_unique<Impl>()) {
    impl_->aff_path = std::move(aff_path);
    impl_->dic_path = std::move(dic_path);

#if HERMES_INTERNAL_HAS_HUNSPELL
    if (!impl_->aff_path.empty() && !impl_->dic_path.empty() &&
        std::filesystem::exists(impl_->aff_path) &&
        std::filesystem::exists(impl_->dic_path)) {
        impl_->handle = Hunspell_create(
            impl_->aff_path.string().c_str(),
            impl_->dic_path.string().c_str());
    }
#endif
}

HunspellSpellService::~HunspellSpellService() {
#if HERMES_INTERNAL_HAS_HUNSPELL
    if (impl_ && impl_->handle) {
        Hunspell_destroy(impl_->handle);
    }
#endif
}

bool HunspellSpellService::IsAvailable() const {
#if HERMES_INTERNAL_HAS_HUNSPELL
    return impl_ && impl_->handle;
#else
    return false;
#endif
}

std::string HunspellSpellService::EngineName() const {
    return "Hunspell";
}

std::vector<SpellIssue> HunspellSpellService::Check(std::string_view text, const SpellCheckRequest&) {
    std::vector<SpellIssue> issues;
    if (!IsAvailable()) {
        return issues;
    }

    std::size_t offset = 0;
    while (offset < text.size()) {
        while (offset < text.size() && !std::isalpha(static_cast<unsigned char>(text[offset]))) {
            ++offset;
        }

        const std::size_t word_start = offset;
        while (offset < text.size() && std::isalpha(static_cast<unsigned char>(text[offset]))) {
            ++offset;
        }

        if (word_start == offset) {
            continue;
        }

        const std::string word(text.substr(word_start, offset - word_start));
        if (impl_->ignored_words.count(ToLower(word)) != 0) {
            continue;
        }

#if HERMES_INTERNAL_HAS_HUNSPELL
        if (!Hunspell_spell(impl_->handle, word.c_str())) {
            issues.push_back(SpellIssue{word_start, word.size(), word});
        }
#endif
    }

    return issues;
}

std::vector<std::string> HunspellSpellService::Suggest(std::string_view word, const SpellCheckRequest&) {
    std::vector<std::string> suggestions;
    if (!IsAvailable()) {
        return suggestions;
    }

#if HERMES_INTERNAL_HAS_HUNSPELL
    char** list = nullptr;
    const int count = Hunspell_suggest(impl_->handle, &list, std::string(word).c_str());
    for (int i = 0; i < count; ++i) {
        suggestions.emplace_back(list[i]);
    }
    if (list != nullptr) {
        Hunspell_free_list(impl_->handle, &list, count);
    }
#endif

    return suggestions;
}

bool HunspellSpellService::AddWordToUserDictionary(std::string_view word, const SpellCheckRequest&) {
    if (!IsAvailable()) {
        return false;
    }

#if HERMES_INTERNAL_HAS_HUNSPELL
    return Hunspell_add(impl_->handle, std::string(word).c_str()) == 0;
#else
    return false;
#endif
}

void HunspellSpellService::IgnoreWord(std::string_view word) {
    impl_->ignored_words.insert(ToLower(word));
}

}  // namespace hermes
