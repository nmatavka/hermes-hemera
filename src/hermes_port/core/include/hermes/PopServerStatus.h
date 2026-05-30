#pragma once

#include <string>
#include <string_view>

namespace hermes {

enum class PopServerStatus {
    kNone = 0,
    kLeave = 1,
    kFetch = 2,
    kDelete = 3,
    kFetchDelete = 4,
};

std::string ToString(PopServerStatus status);
PopServerStatus PopServerStatusFromString(std::string_view value,
                                         PopServerStatus fallback = PopServerStatus::kNone);
std::string PopServerStatusLabel(PopServerStatus status);

}  // namespace hermes
