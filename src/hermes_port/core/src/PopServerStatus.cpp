#include "hermes/PopServerStatus.h"

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

std::string ToString(PopServerStatus status) {
    switch (status) {
        case PopServerStatus::kNone:
            return "none";
        case PopServerStatus::kLeave:
            return "leave";
        case PopServerStatus::kFetch:
            return "fetch";
        case PopServerStatus::kDelete:
            return "delete";
        case PopServerStatus::kFetchDelete:
            return "fetch-delete";
    }
    return "none";
}

PopServerStatus PopServerStatusFromString(std::string_view value, PopServerStatus fallback) {
    const std::string normalized = Normalize(value);
    if (normalized.empty() || normalized == "none") {
        return PopServerStatus::kNone;
    }
    if (normalized == "leave") {
        return PopServerStatus::kLeave;
    }
    if (normalized == "fetch") {
        return PopServerStatus::kFetch;
    }
    if (normalized == "delete") {
        return PopServerStatus::kDelete;
    }
    if (normalized == "fetch-delete" || normalized == "fetch_delete" ||
        normalized == "fetch delete" || normalized == "fetch+delete") {
        return PopServerStatus::kFetchDelete;
    }
    return fallback;
}

std::string PopServerStatusLabel(PopServerStatus status) {
    switch (status) {
        case PopServerStatus::kNone:
            return {};
        case PopServerStatus::kLeave:
            return "Leave";
        case PopServerStatus::kFetch:
            return "Fetch";
        case PopServerStatus::kDelete:
            return "Delete";
        case PopServerStatus::kFetchDelete:
            return "Fetch/Delete";
    }
    return {};
}

}  // namespace hermes
