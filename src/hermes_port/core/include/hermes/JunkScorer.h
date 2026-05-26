#pragma once

#include <string>
#include <vector>

#include "hermes/AddressBookService.h"
#include "hermes/MessageStore.h"

namespace hermes {

struct JunkVerdict {
    bool is_junk = false;
    int score = 0;
    std::vector<std::string> reasons;
};

class JunkScorer {
public:
    virtual ~JunkScorer() = default;

    virtual JunkVerdict Score(const MessageRecord& message) const = 0;
};

class HeuristicJunkScorer final : public JunkScorer {
public:
    explicit HeuristicJunkScorer(const AddressBookService* address_book = nullptr);

    JunkVerdict Score(const MessageRecord& message) const override;
    void AddKeyword(std::string keyword);

private:
    const AddressBookService* address_book_ = nullptr;
    std::vector<std::string> keywords_;
};

}  // namespace hermes
