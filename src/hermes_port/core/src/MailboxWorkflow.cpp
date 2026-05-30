#include "hermes/MailboxWorkflow.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <functional>
#include <set>
#include <sstream>

namespace hermes {

namespace {

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string Normalize(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    std::transform(value.begin(),
                   value.end(),
                   std::back_inserter(normalized),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized;
}

bool IsJunkMailbox(const MailboxRecord& mailbox) {
    const std::string lowered_id = Normalize(mailbox.id);
    const std::string lowered_name = Normalize(mailbox.display_name);
    const std::string lowered_remote = Normalize(mailbox.remote_name);
    return lowered_id == "junk" || lowered_name == "junk" || lowered_remote == "junk";
}

bool IsInboxMailbox(const MailboxRecord& mailbox) {
    return Normalize(mailbox.id) == "inbox" || Normalize(mailbox.display_name) == "inbox" ||
           Normalize(mailbox.display_name) == "in";
}

std::string ExtractEmailAddress(std::string_view value) {
    const std::size_t lt = value.find('<');
    const std::size_t gt = value.find('>', lt == std::string_view::npos ? 0 : lt + 1);
    if (lt != std::string_view::npos && gt != std::string_view::npos && gt > lt + 1) {
        return Trim(std::string(value.substr(lt + 1, gt - lt - 1)));
    }
    return Trim(std::string(value));
}

std::vector<std::string> SplitRecipientAddresses(std::string_view value) {
    std::vector<std::string> addresses;
    std::stringstream stream{std::string(value)};
    std::string token;
    while (std::getline(stream, token, ',')) {
        token = ExtractEmailAddress(token);
        if (!token.empty()) {
            addresses.push_back(token);
        }
    }
    return addresses;
}

std::string StripReplyPrefixes(std::string subject) {
    for (;;) {
        std::string lowered = Normalize(subject);
        if (lowered.rfind("re:", 0) == 0) {
            subject = Trim(subject.substr(3));
            continue;
        }
        if (lowered.rfind("fwd:", 0) == 0) {
            subject = Trim(subject.substr(4));
            continue;
        }
        if (lowered.rfind("fw:", 0) == 0) {
            subject = Trim(subject.substr(3));
            continue;
        }
        break;
    }
    return subject;
}

char TocStateForLegacyStatus(LegacyMessageStatus status) {
    switch (status) {
        case LegacyMessageStatus::kUnread:
            return 0;
        case LegacyMessageStatus::kRead:
            return 1;
        case LegacyMessageStatus::kReplied:
            return 2;
        case LegacyMessageStatus::kForwarded:
            return 3;
        case LegacyMessageStatus::kQueued:
            return 7;
        case LegacyMessageStatus::kSent:
            return 8;
        case LegacyMessageStatus::kUnsent:
            return 9;
        case LegacyMessageStatus::kRedirected:
        case LegacyMessageStatus::kUnsendable:
        case LegacyMessageStatus::kSendable:
        case LegacyMessageStatus::kTimeQueued:
        case LegacyMessageStatus::kSpooled:
        case LegacyMessageStatus::kRecovered:
            return 1;
    }
    return 1;
}

std::string ErrorForMessageId(std::string_view prefix, std::string_view message_id) {
    return std::string(prefix) + std::string(message_id);
}

bool SaveUpdatedMessage(MessageStore& message_store,
                        std::string_view mailbox_id,
                        std::string_view message_id,
                        const std::function<void(MessageRecord&)>& update,
                        std::string* error_message) {
    auto message = message_store.GetMessage(mailbox_id, message_id);
    if (!message) {
        if (error_message) {
            *error_message = ErrorForMessageId("Unable to locate message ", message_id);
        }
        return false;
    }
    update(*message);
    message->updated_at = std::time(nullptr);
    return message_store.SaveMessage(*message, error_message);
}

bool EnsureLocalMailbox(MailboxStore& mailbox_store,
                        std::string_view mailbox_id,
                        std::string_view display_name,
                        std::string_view account_id,
                        std::string* error_message) {
    return mailbox_store.EnsureMailbox({std::string(mailbox_id),
                                        std::string(display_name),
                                        {},
                                        std::string(account_id),
                                        MailboxProtocol::kLocal,
                                        "",
                                        false,
                                        mailbox_id == "junk" || mailbox_id == "inbox",
                                        0,
                                        MailboxKind::kMailbox,
                                        {}},
                                       error_message);
}

FilterResult EvaluateFilters(const FilterStore& filter_store, const MessageRecord& message) {
    RuleBasedFilterEngine engine;
    engine.SetRules(filter_store.Rules());
    return engine.Evaluate(message);
}

void RecordFilterResult(FilterReportStore& report_store,
                        const MailboxRecord& source_mailbox,
                        const MessageRecord& message,
                        const FilterResult& result) {
    if (!result.matched) {
        return;
    }
    const auto now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    report_store.AddEntry({"filter-" + message.id + "-" + std::to_string(now),
                           message.id,
                           source_mailbox.id,
                           source_mailbox.display_name,
                           message.sender,
                           message.subject,
                           result.matched_rules,
                           static_cast<std::int64_t>(now)});
}

bool MoveLocalMessage(MessageStore& message_store,
                      MailboxStore& mailbox_store,
                      const MessageRecord& message,
                      const MailboxRecord& source_mailbox,
                      std::string_view destination_mailbox_id,
                      std::string* error_message) {
    if (destination_mailbox_id.empty() || destination_mailbox_id == source_mailbox.id) {
        return true;
    }
    std::string display_name = std::string(destination_mailbox_id);
    if (destination_mailbox_id == "inbox") {
        display_name = "In";
    } else if (destination_mailbox_id == "junk") {
        display_name = "Junk";
    }
    if (!EnsureLocalMailbox(mailbox_store, destination_mailbox_id, display_name, message.account_id, error_message)) {
        return false;
    }
    return message_store.MoveMessage(source_mailbox.id, message.id, destination_mailbox_id, error_message);
}

std::optional<std::string> CommonSender(const std::vector<MessageRecord>& messages) {
    if (messages.empty()) {
        return std::nullopt;
    }
    const std::string first = ExtractEmailAddress(messages.front().sender);
    if (first.empty()) {
        return std::nullopt;
    }
    for (const auto& message : messages) {
        if (Normalize(ExtractEmailAddress(message.sender)) != Normalize(first)) {
            return std::nullopt;
        }
    }
    return first;
}

std::optional<std::string> CommonRecipient(const std::vector<MessageRecord>& messages) {
    if (messages.empty()) {
        return std::nullopt;
    }

    std::set<std::string> common;
    bool first_message = true;
    for (const auto& message : messages) {
        std::set<std::string> addresses;
        for (const auto& address : SplitRecipientAddresses(message.recipients)) {
            addresses.insert(Normalize(address));
        }
        if (first_message) {
            common = std::move(addresses);
            first_message = false;
        } else {
            std::set<std::string> intersection;
            std::set_intersection(common.begin(),
                                  common.end(),
                                  addresses.begin(),
                                  addresses.end(),
                                  std::inserter(intersection, intersection.begin()));
            common = std::move(intersection);
        }
        if (common.empty()) {
            return std::nullopt;
        }
    }

    return common.empty() ? std::nullopt : std::optional<std::string>(*common.begin());
}

std::optional<std::string> CommonSubject(const std::vector<MessageRecord>& messages) {
    if (messages.empty()) {
        return std::nullopt;
    }
    const std::string first = StripReplyPrefixes(messages.front().subject);
    if (first.empty()) {
        return std::nullopt;
    }
    for (const auto& message : messages) {
        if (Normalize(StripReplyPrefixes(message.subject)) != Normalize(first)) {
            return std::nullopt;
        }
    }
    return first;
}

std::string SuggestedFilterName(std::string_view value) {
    const std::string trimmed = Trim(std::string(value));
    return trimmed.empty() ? "New Filter" : "Filter " + trimmed;
}

}  // namespace

std::optional<std::string> DefaultRecentMailboxId(const MailboxStore& mailbox_store) {
    std::optional<std::string> fallback;
    for (const auto& mailbox : mailbox_store.ListMailboxes()) {
        if (mailbox.kind != MailboxKind::kMailbox) {
            continue;
        }
        if (IsInboxMailbox(mailbox)) {
            return mailbox.id;
        }
        if (!fallback) {
            fallback = mailbox.id;
        }
    }
    return fallback;
}

std::vector<std::string> NormalizeRecentMailboxIds(const std::vector<std::string>& stored_ids,
                                                   const MailboxStore& mailbox_store,
                                                   int max_recent) {
    std::vector<std::string> normalized;
    if (max_recent <= 0) {
        return normalized;
    }

    normalized.reserve(static_cast<std::size_t>(max_recent));
    for (const auto& mailbox_id : stored_ids) {
        if (mailbox_id.empty() ||
            std::find(normalized.begin(), normalized.end(), mailbox_id) != normalized.end()) {
            continue;
        }
        const auto mailbox = mailbox_store.GetMailbox(mailbox_id);
        if (!mailbox || mailbox->kind != MailboxKind::kMailbox) {
            continue;
        }
        normalized.push_back(mailbox->id);
        if (normalized.size() >= static_cast<std::size_t>(max_recent)) {
            break;
        }
    }

    if (normalized.empty()) {
        if (const auto fallback = DefaultRecentMailboxId(mailbox_store)) {
            normalized.push_back(*fallback);
        }
    }
    return normalized;
}

std::vector<std::string> RememberRecentMailboxId(const std::vector<std::string>& stored_ids,
                                                 const MailboxStore& mailbox_store,
                                                 std::string_view mailbox_id,
                                                 int max_recent) {
    if (max_recent <= 0) {
        return {};
    }

    std::vector<std::string> merged;
    merged.reserve(stored_ids.size() + 1);
    if (!mailbox_id.empty()) {
        if (const auto mailbox = mailbox_store.GetMailbox(mailbox_id);
            mailbox && mailbox->kind == MailboxKind::kMailbox) {
            merged.push_back(mailbox->id);
        }
    }
    merged.insert(merged.end(), stored_ids.begin(), stored_ids.end());
    return NormalizeRecentMailboxIds(merged, mailbox_store, max_recent);
}

bool SetLegacyStatus(MessageStore& message_store,
                     std::string_view mailbox_id,
                     const std::vector<std::string>& message_ids,
                     LegacyMessageStatus status,
                     std::string* error_message) {
    for (const auto& message_id : message_ids) {
        if (!SaveUpdatedMessage(message_store, mailbox_id, message_id, [&](MessageRecord& message) {
                message.legacy_status = status;
                message.unread = LegacyMessageStatusIsUnread(status);
            }, error_message)) {
            return false;
        }
    }
    return true;
}

bool SetLabel(MessageStore& message_store,
              std::string_view mailbox_id,
              const std::vector<std::string>& message_ids,
              int label_index,
              std::string* error_message) {
    for (const auto& message_id : message_ids) {
        if (!SaveUpdatedMessage(message_store, mailbox_id, message_id, [&](MessageRecord& message) {
                message.label_index = std::max(0, label_index);
            }, error_message)) {
            return false;
        }
    }
    return true;
}

PopServerStatus EffectivePopServerStatus(const MailboxRecord& mailbox,
                                         PopServerStatus requested_status,
                                         bool delete_fetched_junk) {
    if (requested_status == PopServerStatus::kFetch && delete_fetched_junk && IsJunkMailbox(mailbox)) {
        return PopServerStatus::kDelete;
    }
    return requested_status;
}

bool SetPopServerStatus(MessageStore& message_store,
                        const MailboxRecord& mailbox,
                        const std::vector<std::string>& message_ids,
                        PopServerStatus status,
                        bool delete_fetched_junk,
                        std::string* error_message) {
    const PopServerStatus effective_status =
        EffectivePopServerStatus(mailbox, status, delete_fetched_junk);
    for (const auto& message_id : message_ids) {
        if (!SaveUpdatedMessage(message_store, mailbox.id, message_id, [&](MessageRecord& message) {
                message.pop_server_status = effective_status;
            }, error_message)) {
            return false;
        }
    }
    return true;
}

bool ApplyFiltersToMessages(MessageStore& message_store,
                            MailboxStore& mailbox_store,
                            FilterReportStore& report_store,
                            const MailboxRecord& mailbox,
                            const std::vector<std::string>& message_ids,
                            const FilterStore& filter_store,
                            std::string* error_message) {
    for (const auto& message_id : message_ids) {
        auto message = message_store.GetMessage(mailbox.id, message_id);
        if (!message) {
            if (error_message) {
                *error_message = ErrorForMessageId("Unable to locate message ", message_id);
            }
            return false;
        }

        const auto result = EvaluateFilters(filter_store, *message);
        message->filters_applied = true;
        if (result.mark_as_read) {
            message->unread = false;
            message->legacy_status = LegacyMessageStatus::kRead;
        }
        if (result.mark_as_junk) {
            message->manually_junked = true;
            message->junk_score = 100;
        }
        if (!message_store.SaveMessage(*message, error_message)) {
            return false;
        }

        RecordFilterResult(report_store, mailbox, *message, result);

        std::string destination_mailbox = mailbox.id;
        if (result.mark_as_junk) {
            destination_mailbox = "junk";
        }
        if (result.destination_mailbox) {
            destination_mailbox = *result.destination_mailbox;
        }
        if (!MoveLocalMessage(message_store, mailbox_store, *message, mailbox, destination_mailbox, error_message)) {
            return false;
        }
    }
    return true;
}

bool ApplyJunkActionToLocalMessages(MessageStore& message_store,
                                    MailboxStore& mailbox_store,
                                    FilterReportStore& report_store,
                                    const MailboxRecord& mailbox,
                                    const std::vector<std::string>& message_ids,
                                    MailboxJunkAction action,
                                    const JunkScorer& junk_scorer,
                                    const FilterStore& filter_store,
                                    std::string* error_message) {
    for (const auto& message_id : message_ids) {
        auto message = message_store.GetMessage(mailbox.id, message_id);
        if (!message) {
            if (error_message) {
                *error_message = ErrorForMessageId("Unable to locate message ", message_id);
            }
            return false;
        }

        std::string destination_mailbox = mailbox.id;
        FilterResult filter_result;

        switch (action) {
            case MailboxJunkAction::kJunk:
                message->manually_junked = true;
                message->junk_score = 100;
                destination_mailbox = "junk";
                break;

            case MailboxJunkAction::kNotJunk:
                message->manually_junked = false;
                message->junk_score = 0;
                destination_mailbox = "inbox";
                filter_result = EvaluateFilters(filter_store, *message);
                if (filter_result.mark_as_junk) {
                    destination_mailbox = "junk";
                }
                if (filter_result.destination_mailbox) {
                    destination_mailbox = *filter_result.destination_mailbox;
                }
                break;

            case MailboxJunkAction::kRecheck: {
                const auto verdict = junk_scorer.Score(*message);
                message->manually_junked = false;
                message->junk_score = std::clamp(verdict.score, 0, 127);
                filter_result = EvaluateFilters(filter_store, *message);
                if (verdict.is_junk || filter_result.mark_as_junk) {
                    destination_mailbox = "junk";
                }
                if (filter_result.destination_mailbox) {
                    destination_mailbox = *filter_result.destination_mailbox;
                }
                break;
            }
        }

        if (!message_store.SaveMessage(*message, error_message)) {
            return false;
        }
        RecordFilterResult(report_store, mailbox, *message, filter_result);
        if (!MoveLocalMessage(message_store, mailbox_store, *message, mailbox, destination_mailbox, error_message)) {
            return false;
        }
    }
    return true;
}

std::optional<ManualFilterSuggestion> SuggestManualFilter(const std::vector<MessageRecord>& messages) {
    if (messages.empty()) {
        return std::nullopt;
    }

    ManualFilterSuggestion suggestion;
    if (const auto sender = CommonSender(messages)) {
        suggestion.rule.field = FilterField::kFrom;
        suggestion.rule.operation = FilterOperation::kContains;
        suggestion.rule.value = *sender;
        suggestion.suggested_destination_name = *sender;
    } else if (const auto recipient = CommonRecipient(messages)) {
        suggestion.rule.field = FilterField::kTo;
        suggestion.rule.operation = FilterOperation::kContains;
        suggestion.rule.value = *recipient;
        suggestion.suggested_destination_name = *recipient;
    } else if (const auto subject = CommonSubject(messages)) {
        suggestion.rule.field = FilterField::kSubject;
        suggestion.rule.operation = FilterOperation::kContains;
        suggestion.rule.value = *subject;
        suggestion.suggested_destination_name = *subject;
    } else {
        return std::nullopt;
    }

    suggestion.rule.name = SuggestedFilterName(suggestion.suggested_destination_name);
    suggestion.rule.stop_processing = true;
    if (!suggestion.suggested_destination_name.empty()) {
        suggestion.rule.destination_mailbox = suggestion.suggested_destination_name;
    }
    return suggestion;
}

}  // namespace hermes
