#include "hermes/SearchService.h"

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

std::size_t CountMatches(std::string_view haystack, std::string_view needle, bool match_case) {
    if (needle.empty()) {
        return 0;
    }

    std::string normalized_haystack = match_case ? std::string(haystack) : Normalize(haystack);
    std::string normalized_needle = match_case ? std::string(needle) : Normalize(needle);
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = normalized_haystack.find(normalized_needle, position)) != std::string::npos) {
        ++count;
        position += normalized_needle.size();
    }
    return count;
}

std::string BuildSnippet(std::string_view text, std::string_view needle, bool match_case) {
    if (text.empty() || needle.empty()) {
        return {};
    }

    const std::string source = std::string(text);
    const std::string matchable = match_case ? source : Normalize(source);
    const std::string token = match_case ? std::string(needle) : Normalize(needle);
    const std::size_t position = matchable.find(token);
    if (position == std::string::npos) {
        return {};
    }

    const std::size_t start = position > 24 ? position - 24 : 0;
    const std::size_t length = std::min<std::size_t>(source.size() - start, token.size() + 48);
    return source.substr(start, length);
}

}  // namespace

std::vector<SearchHit> SimpleSearchService::Search(const std::vector<MessageRecord>& messages,
                                                   const SearchQuery& query) const {
    std::vector<SearchHit> hits;
    if (query.term.empty()) {
        return hits;
    }

    for (const auto& message : messages) {
        std::size_t score = 0;
        std::string snippet;

        if (query.search_subject) {
            const std::size_t matches = CountMatches(message.subject, query.term, query.match_case);
            score += matches * 5;
            if (snippet.empty() && matches != 0) {
                snippet = BuildSnippet(message.subject, query.term, query.match_case);
            }
        }

        if (query.search_headers) {
            const std::size_t sender_matches = CountMatches(message.sender, query.term, query.match_case);
            const std::size_t recipient_matches =
                CountMatches(message.recipients, query.term, query.match_case);
            score += sender_matches * 4;
            score += recipient_matches * 3;
            if (snippet.empty() && sender_matches != 0) {
                snippet = BuildSnippet(message.sender, query.term, query.match_case);
            }
            if (snippet.empty() && recipient_matches != 0) {
                snippet = BuildSnippet(message.recipients, query.term, query.match_case);
            }
        }

        if (query.search_body) {
            const std::string body = message.plain_text_body.empty() ? message.html_body : message.plain_text_body;
            const std::size_t matches = CountMatches(body, query.term, query.match_case);
            score += matches;
            if (snippet.empty() && matches != 0) {
                snippet = BuildSnippet(body, query.term, query.match_case);
            }
        }

        if (score == 0) {
            continue;
        }

        hits.push_back(SearchHit{message.id, message.mailbox_id, score, snippet});
    }

    std::sort(hits.begin(),
              hits.end(),
              [](const SearchHit& left, const SearchHit& right) {
                  if (left.score != right.score) {
                      return left.score > right.score;
                  }
                  if (left.mailbox_id != right.mailbox_id) {
                      return left.mailbox_id < right.mailbox_id;
                  }
                  return left.message_id < right.message_id;
              });
    return hits;
}

}  // namespace hermes
