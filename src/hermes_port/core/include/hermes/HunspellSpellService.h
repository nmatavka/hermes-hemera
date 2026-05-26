#pragma once

#include <filesystem>
#include <memory>

#include "hermes/SpellService.h"

namespace hermes {

class HunspellSpellService final : public SpellService {
public:
    HunspellSpellService(std::filesystem::path aff_path, std::filesystem::path dic_path);
    ~HunspellSpellService() override;

    HunspellSpellService(const HunspellSpellService&) = delete;
    HunspellSpellService& operator=(const HunspellSpellService&) = delete;

    bool IsAvailable() const override;
    std::string EngineName() const override;
    std::vector<SpellIssue> Check(std::string_view text, const SpellCheckRequest& request) override;
    std::vector<std::string> Suggest(std::string_view word, const SpellCheckRequest& request) override;
    bool AddWordToUserDictionary(std::string_view word, const SpellCheckRequest& request) override;
    void IgnoreWord(std::string_view word) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace hermes
