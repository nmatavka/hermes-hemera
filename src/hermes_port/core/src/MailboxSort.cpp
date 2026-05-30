#include "hermes/MailboxSort.h"

#include <algorithm>
#include <cctype>

namespace hermes {

namespace {

std::string Trim(std::string_view value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return std::string(value.substr(start, end - start));
}

std::string Lower(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    std::transform(value.begin(),
                   value.end(),
                   std::back_inserter(lowered),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
}

}  // namespace

std::string NormalizeMailboxSubject(std::string_view subject) {
    std::string normalized = Trim(subject);
    for (;;) {
        const std::string lowered = Lower(normalized);
        if (lowered.rfind("re:", 0) == 0) {
            normalized = Trim(normalized.substr(3));
            continue;
        }
        if (lowered.rfind("fw:", 0) == 0) {
            normalized = Trim(normalized.substr(3));
            continue;
        }
        if (lowered.rfind("fwd:", 0) == 0) {
            normalized = Trim(normalized.substr(4));
            continue;
        }
        break;
    }
    return Lower(normalized);
}

int CompareMailboxSubjects(std::string_view left, std::string_view right) {
    const std::string normalized_left = NormalizeMailboxSubject(left);
    const std::string normalized_right = NormalizeMailboxSubject(right);
    if (normalized_left < normalized_right) {
        return -1;
    }
    if (normalized_left > normalized_right) {
        return 1;
    }
    return 0;
}

int LegacyMessageStatusSortRank(LegacyMessageStatus status) {
    switch (status) {
        case LegacyMessageStatus::kUnread:
            return 0;
        case LegacyMessageStatus::kRead:
            return 1;
        case LegacyMessageStatus::kReplied:
            return 2;
        case LegacyMessageStatus::kForwarded:
            return 3;
        case LegacyMessageStatus::kRedirected:
            return 4;
        case LegacyMessageStatus::kRecovered:
            return 5;
        case LegacyMessageStatus::kUnsendable:
            return 6;
        case LegacyMessageStatus::kSendable:
            return 7;
        case LegacyMessageStatus::kQueued:
            return 8;
        case LegacyMessageStatus::kTimeQueued:
            return 9;
        case LegacyMessageStatus::kSent:
            return 10;
        case LegacyMessageStatus::kUnsent:
            return 11;
        case LegacyMessageStatus::kSpooled:
            return 12;
    }
    return 0;
}

int PopServerStatusSortRank(PopServerStatus status) {
    switch (status) {
        case PopServerStatus::kNone:
            return 0;
        case PopServerStatus::kLeave:
            return 1;
        case PopServerStatus::kFetch:
            return 2;
        case PopServerStatus::kDelete:
            return 3;
        case PopServerStatus::kFetchDelete:
            return 4;
    }
    return 0;
}

}  // namespace hermes
