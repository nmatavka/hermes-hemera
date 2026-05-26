#pragma once

#include <optional>
#include <string>
#include <vector>

#include "hermes/MessageStore.h"

namespace hermes {

enum class FilterField {
    kFrom,
    kTo,
    kSubject,
    kBody,
};

enum class FilterOperation {
    kContains,
    kEquals,
    kNotContains,
};

struct FilterRule {
    std::string name;
    FilterField field = FilterField::kSubject;
    FilterOperation operation = FilterOperation::kContains;
    std::string value;
    std::optional<std::string> destination_mailbox;
    bool mark_as_read = false;
    bool mark_as_junk = false;
    bool stop_processing = true;
};

struct FilterResult {
    bool matched = false;
    bool mark_as_read = false;
    bool mark_as_junk = false;
    std::optional<std::string> destination_mailbox;
    std::vector<std::string> matched_rules;
};

class FilterEngine {
public:
    virtual ~FilterEngine() = default;

    virtual void SetRules(std::vector<FilterRule> rules) = 0;
    virtual void AddRule(const FilterRule& rule) = 0;
    virtual FilterResult Evaluate(const MessageRecord& message) const = 0;
};

class RuleBasedFilterEngine final : public FilterEngine {
public:
    void SetRules(std::vector<FilterRule> rules) override;
    void AddRule(const FilterRule& rule) override;
    FilterResult Evaluate(const MessageRecord& message) const override;

private:
    std::vector<FilterRule> rules_;
};

}  // namespace hermes
