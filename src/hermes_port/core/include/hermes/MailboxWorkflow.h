#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/FilterReportStore.h"
#include "hermes/FilterStore.h"
#include "hermes/JunkScorer.h"
#include "hermes/LegacyMessageStatus.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"
#include "hermes/PopServerStatus.h"

namespace hermes {

enum class MailboxJunkAction {
    kJunk,
    kNotJunk,
    kRecheck,
};

struct ManualFilterSuggestion {
    FilterRule rule;
    std::string suggested_destination_name;
};

std::optional<std::string> DefaultRecentMailboxId(const MailboxStore& mailbox_store);
std::vector<std::string> NormalizeRecentMailboxIds(const std::vector<std::string>& stored_ids,
                                                   const MailboxStore& mailbox_store,
                                                   int max_recent);
std::vector<std::string> RememberRecentMailboxId(const std::vector<std::string>& stored_ids,
                                                 const MailboxStore& mailbox_store,
                                                 std::string_view mailbox_id,
                                                 int max_recent);
bool SetLegacyStatus(MessageStore& message_store,
                     std::string_view mailbox_id,
                     const std::vector<std::string>& message_ids,
                     LegacyMessageStatus status,
                     std::string* error_message = nullptr);
bool SetLabel(MessageStore& message_store,
              std::string_view mailbox_id,
              const std::vector<std::string>& message_ids,
              int label_index,
              std::string* error_message = nullptr);
PopServerStatus EffectivePopServerStatus(const MailboxRecord& mailbox,
                                         PopServerStatus requested_status,
                                         bool delete_fetched_junk);
bool SetPopServerStatus(MessageStore& message_store,
                        const MailboxRecord& mailbox,
                        const std::vector<std::string>& message_ids,
                        PopServerStatus status,
                        bool delete_fetched_junk,
                        std::string* error_message = nullptr);
bool ApplyFiltersToMessages(MessageStore& message_store,
                            MailboxStore& mailbox_store,
                            FilterReportStore& report_store,
                            const MailboxRecord& mailbox,
                            const std::vector<std::string>& message_ids,
                            const FilterStore& filter_store,
                            std::string* error_message = nullptr);
bool ApplyJunkActionToLocalMessages(MessageStore& message_store,
                                    MailboxStore& mailbox_store,
                                    FilterReportStore& report_store,
                                    const MailboxRecord& mailbox,
                                    const std::vector<std::string>& message_ids,
                                    MailboxJunkAction action,
                                    const JunkScorer& junk_scorer,
                                    const FilterStore& filter_store,
                                    std::string* error_message = nullptr);
std::optional<ManualFilterSuggestion> SuggestManualFilter(const std::vector<MessageRecord>& messages);

}  // namespace hermes
