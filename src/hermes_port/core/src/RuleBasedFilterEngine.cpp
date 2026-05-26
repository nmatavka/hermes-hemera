#include "hermes/FilterEngine.h"

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

std::string FieldValue(const MessageRecord& message, FilterField field) {
    switch (field) {
        case FilterField::kFrom:
            return message.sender;
        case FilterField::kTo:
            return message.recipients;
        case FilterField::kSubject:
            return message.subject;
        case FilterField::kBody:
            return message.plain_text_body + "\n" + message.html_body;
    }
    return {};
}

bool RuleMatches(const FilterRule& rule, const MessageRecord& message) {
    const std::string field = Normalize(FieldValue(message, rule.field));
    const std::string value = Normalize(rule.value);
    switch (rule.operation) {
        case FilterOperation::kContains:
            return field.find(value) != std::string::npos;
        case FilterOperation::kEquals:
            return field == value;
        case FilterOperation::kNotContains:
            return field.find(value) == std::string::npos;
    }
    return false;
}

}  // namespace

void RuleBasedFilterEngine::SetRules(std::vector<FilterRule> rules) {
    rules_ = std::move(rules);
}

void RuleBasedFilterEngine::AddRule(const FilterRule& rule) {
    rules_.push_back(rule);
}

FilterResult RuleBasedFilterEngine::Evaluate(const MessageRecord& message) const {
    FilterResult result;
    for (const auto& rule : rules_) {
        if (!RuleMatches(rule, message)) {
            continue;
        }

        result.matched = true;
        result.mark_as_read = result.mark_as_read || rule.mark_as_read;
        result.mark_as_junk = result.mark_as_junk || rule.mark_as_junk;
        if (rule.destination_mailbox) {
            result.destination_mailbox = rule.destination_mailbox;
        }
        result.matched_rules.push_back(rule.name.empty() ? rule.value : rule.name);
        if (rule.stop_processing) {
            break;
        }
    }
    return result;
}

}  // namespace hermes
