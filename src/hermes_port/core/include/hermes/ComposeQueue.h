#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "hermes/ComposeController.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"

namespace hermes {

struct QueueComposeResult {
    bool queued = false;
    ComposeSendValidation validation;
    std::optional<MessageRecord> queued_message;
    std::string error_message;
};

QueueComposeResult QueueComposeMessage(ComposeController& controller,
                                       MailboxStore& mailbox_store,
                                       MessageStore& message_store,
                                       std::string_view mailbox_id = "out",
                                       bool allow_warnings = false);

}  // namespace hermes
