#include "hermes/JunkScorer.h"

#include <algorithm>
#include <cctype>

namespace hermes {

namespace {

std::string Normalize(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    std::transform(value.begin(),
                   value.end(),
                   std::back_inserter(normalized),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized;
}

bool Contains(std::string_view haystack, std::string_view needle) {
    return Normalize(haystack).find(Normalize(needle)) != std::string::npos;
}

bool IsMostlyUpper(std::string_view value) {
    int uppercase = 0;
    int alpha = 0;
    for (char ch : value) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            ++alpha;
            if (std::isupper(static_cast<unsigned char>(ch))) {
                ++uppercase;
            }
        }
    }
    return alpha >= 8 && uppercase * 100 / alpha >= 70;
}

}  // namespace

HeuristicJunkScorer::HeuristicJunkScorer(const AddressBookService* address_book)
    : address_book_(address_book),
      keywords_({"viagra", "lottery", "winner", "urgent", "credit", "debt", "casino", "free money",
                 "click here"}) {}

JunkVerdict HeuristicJunkScorer::Score(const MessageRecord& message) const {
    JunkVerdict verdict;

    if (address_book_ != nullptr && address_book_->Contains(message.sender)) {
        verdict.reasons.push_back("sender is in the address book");
        return verdict;
    }

    const std::string subject = Normalize(message.subject);
    const std::string body = Normalize(message.plain_text_body + "\n" + message.html_body);

    for (const auto& keyword : keywords_) {
        if (keyword.empty()) {
            continue;
        }

        const bool subject_match = subject.find(keyword) != std::string::npos;
        const bool body_match = body.find(keyword) != std::string::npos;
        if (subject_match) {
            verdict.score += 10;
            verdict.reasons.push_back("subject contains '" + keyword + "'");
        } else if (body_match) {
            verdict.score += 5;
            verdict.reasons.push_back("body contains '" + keyword + "'");
        }
    }

    if (Contains(message.sender, "noreply") || Contains(message.sender, "no-reply")) {
        verdict.score += 2;
        verdict.reasons.push_back("sender uses an automated mailbox");
    }

    if (IsMostlyUpper(message.subject)) {
        verdict.score += 4;
        verdict.reasons.push_back("subject is mostly uppercase");
    }

    verdict.is_junk = verdict.score >= 10;
    return verdict;
}

void HeuristicJunkScorer::AddKeyword(std::string keyword) {
    keyword = Normalize(keyword);
    if (!keyword.empty()) {
        keywords_.push_back(std::move(keyword));
    }
}

}  // namespace hermes
