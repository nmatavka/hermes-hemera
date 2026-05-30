#include "hermes/LegacyMessageStatus.h"

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

}  // namespace

std::string ToString(LegacyMessageStatus status) {
    switch (status) {
        case LegacyMessageStatus::kUnread:
            return "unread";
        case LegacyMessageStatus::kRead:
            return "read";
        case LegacyMessageStatus::kReplied:
            return "replied";
        case LegacyMessageStatus::kForwarded:
            return "forwarded";
        case LegacyMessageStatus::kRedirected:
            return "redirected";
        case LegacyMessageStatus::kUnsendable:
            return "unsendable";
        case LegacyMessageStatus::kSendable:
            return "sendable";
        case LegacyMessageStatus::kQueued:
            return "queued";
        case LegacyMessageStatus::kSent:
            return "sent";
        case LegacyMessageStatus::kUnsent:
            return "unsent";
        case LegacyMessageStatus::kTimeQueued:
            return "time-queued";
        case LegacyMessageStatus::kSpooled:
            return "spooled";
        case LegacyMessageStatus::kRecovered:
            return "recovered";
    }
    return "unread";
}

LegacyMessageStatus LegacyMessageStatusFromString(std::string_view value, LegacyMessageStatus fallback) {
    const std::string normalized = Normalize(value);
    if (normalized == "unread") {
        return LegacyMessageStatus::kUnread;
    }
    if (normalized == "read") {
        return LegacyMessageStatus::kRead;
    }
    if (normalized == "replied") {
        return LegacyMessageStatus::kReplied;
    }
    if (normalized == "forwarded") {
        return LegacyMessageStatus::kForwarded;
    }
    if (normalized == "redirected") {
        return LegacyMessageStatus::kRedirected;
    }
    if (normalized == "unsendable") {
        return LegacyMessageStatus::kUnsendable;
    }
    if (normalized == "sendable") {
        return LegacyMessageStatus::kSendable;
    }
    if (normalized == "queued") {
        return LegacyMessageStatus::kQueued;
    }
    if (normalized == "sent") {
        return LegacyMessageStatus::kSent;
    }
    if (normalized == "unsent") {
        return LegacyMessageStatus::kUnsent;
    }
    if (normalized == "time-queued" || normalized == "time_queued" || normalized == "time queued") {
        return LegacyMessageStatus::kTimeQueued;
    }
    if (normalized == "spooled") {
        return LegacyMessageStatus::kSpooled;
    }
    if (normalized == "recovered") {
        return LegacyMessageStatus::kRecovered;
    }
    return fallback;
}

std::string LegacyMessageStatusLabel(LegacyMessageStatus status) {
    switch (status) {
        case LegacyMessageStatus::kUnread:
            return "Unread";
        case LegacyMessageStatus::kRead:
            return "Read";
        case LegacyMessageStatus::kReplied:
            return "Replied";
        case LegacyMessageStatus::kForwarded:
            return "Forwarded";
        case LegacyMessageStatus::kRedirected:
            return "Redirected";
        case LegacyMessageStatus::kUnsendable:
            return "Unsendable";
        case LegacyMessageStatus::kSendable:
            return "Sendable";
        case LegacyMessageStatus::kQueued:
            return "Queued";
        case LegacyMessageStatus::kSent:
            return "Sent";
        case LegacyMessageStatus::kUnsent:
            return "Unsent";
        case LegacyMessageStatus::kTimeQueued:
            return "Time Queued";
        case LegacyMessageStatus::kSpooled:
            return "Spooled";
        case LegacyMessageStatus::kRecovered:
            return "Recovered";
    }
    return "Unread";
}

bool LegacyMessageStatusIsUnread(LegacyMessageStatus status) {
    return status == LegacyMessageStatus::kUnread;
}

}  // namespace hermes
