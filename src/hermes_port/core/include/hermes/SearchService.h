#pragma once

#include <string>
#include <vector>

#include "hermes/MessageStore.h"

namespace hermes {

struct SearchQuery {
    std::string term;
    bool match_case = false;
    bool search_subject = true;
    bool search_headers = true;
    bool search_body = true;
};

struct SearchHit {
    std::string message_id;
    std::string mailbox_id;
    std::size_t score = 0;
    std::string snippet;
};

class SearchService {
public:
    virtual ~SearchService() = default;

    virtual std::vector<SearchHit> Search(const std::vector<MessageRecord>& messages,
                                          const SearchQuery& query) const = 0;
};

class SimpleSearchService final : public SearchService {
public:
    std::vector<SearchHit> Search(const std::vector<MessageRecord>& messages,
                                  const SearchQuery& query) const override;
};

}  // namespace hermes
