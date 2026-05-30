#pragma once

#include <string>
#include <string_view>

#include "hermes/LegacyMessageStatus.h"
#include "hermes/PopServerStatus.h"

namespace hermes {

std::string NormalizeMailboxSubject(std::string_view subject);
int CompareMailboxSubjects(std::string_view left, std::string_view right);
int LegacyMessageStatusSortRank(LegacyMessageStatus status);
int PopServerStatusSortRank(PopServerStatus status);

}  // namespace hermes
