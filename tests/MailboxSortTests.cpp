#include "TestRegistry.h"

#include "hermes/LegacyMessageStatus.h"
#include "hermes/MailboxSort.h"
#include "hermes/PopServerStatus.h"

HERMES_TEST(MailboxSortNormalizesReplyPrefixesForSubjectComparison) {
    const auto normalized = hermes::NormalizeMailboxSubject(" Re: Fwd:  Launch review ");
    HERMES_CHECK_EQ(normalized, std::string("launch review"));
    HERMES_CHECK_EQ(hermes::CompareMailboxSubjects("Re: Launch review", "launch review"), 0);
    HERMES_CHECK(hermes::CompareMailboxSubjects("Alpha", "Beta") < 0);
}

HERMES_TEST(MailboxSortRanksLegacyAndPopServerStatusConsistently) {
    HERMES_CHECK(hermes::LegacyMessageStatusSortRank(hermes::LegacyMessageStatus::kUnread) <
                 hermes::LegacyMessageStatusSortRank(hermes::LegacyMessageStatus::kRead));
    HERMES_CHECK(hermes::LegacyMessageStatusSortRank(hermes::LegacyMessageStatus::kQueued) <
                 hermes::LegacyMessageStatusSortRank(hermes::LegacyMessageStatus::kSent));
    HERMES_CHECK(hermes::PopServerStatusSortRank(hermes::PopServerStatus::kLeave) <
                 hermes::PopServerStatusSortRank(hermes::PopServerStatus::kDelete));
    HERMES_CHECK(hermes::PopServerStatusSortRank(hermes::PopServerStatus::kFetch) <
                 hermes::PopServerStatusSortRank(hermes::PopServerStatus::kFetchDelete));
}
