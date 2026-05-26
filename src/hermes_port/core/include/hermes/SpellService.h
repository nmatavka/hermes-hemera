#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace hermes {

struct SpellCheckRequest {
    std::string language = "en_US";
    bool include_quoted_text = true;
    bool interactive = true;
};

struct SpellIssue {
    std::size_t offset = 0;
    std::size_t length = 0;
    std::string word;
};

class SpellService {
public:
    virtual ~SpellService() = default;

    virtual bool IsAvailable() const = 0;
    virtual std::string EngineName() const = 0;
    virtual std::vector<SpellIssue> Check(std::string_view text, const SpellCheckRequest& request) = 0;
    virtual std::vector<std::string> Suggest(std::string_view word, const SpellCheckRequest& request) = 0;
    virtual bool AddWordToUserDictionary(std::string_view word, const SpellCheckRequest& request) = 0;
    virtual void IgnoreWord(std::string_view word) = 0;
};

}  // namespace hermes
