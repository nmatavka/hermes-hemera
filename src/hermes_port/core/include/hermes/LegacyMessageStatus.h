#pragma once

#include <string>
#include <string_view>

namespace hermes {

enum class LegacyMessageStatus {
    kUnread = 0,
    kRead = 1,
    kReplied = 2,
    kForwarded = 3,
    kRedirected = 4,
    kUnsendable = 5,
    kSendable = 6,
    kQueued = 7,
    kSent = 8,
    kUnsent = 9,
    kTimeQueued = 10,
    kSpooled = 11,
    kRecovered = 12,
};

std::string ToString(LegacyMessageStatus status);
LegacyMessageStatus LegacyMessageStatusFromString(std::string_view value,
                                                 LegacyMessageStatus fallback = LegacyMessageStatus::kUnread);
std::string LegacyMessageStatusLabel(LegacyMessageStatus status);
bool LegacyMessageStatusIsUnread(LegacyMessageStatus status);

}  // namespace hermes
