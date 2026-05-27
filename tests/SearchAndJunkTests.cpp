#include "TestRegistry.h"

#include "hermes/AddressBookService.h"
#include "hermes/JunkScorer.h"
#include "hermes/SearchService.h"

HERMES_TEST(SimpleSearchServiceRanksSubjectHitsAboveBodyHits) {
    hermes::SimpleSearchService search;
    hermes::MessageRecord first;
    first.id = "1";
    first.mailbox_id = "inbox";
    first.subject = "Release checklist";
    first.sender = "alice@example.com";
    first.recipients = "team@example.com";
    first.plain_text_body = "No blockers";
    first.unread = true;

    hermes::MessageRecord second;
    second.id = "2";
    second.mailbox_id = "inbox";
    second.subject = "Status";
    second.sender = "bob@example.com";
    second.recipients = "team@example.com";
    second.plain_text_body = "release release release";
    second.unread = true;

    std::vector<hermes::MessageRecord> messages = {first, second};

    const auto hits = search.Search(messages, {"release", false, true, true, true});
    HERMES_CHECK_EQ(hits.size(), static_cast<std::size_t>(2));
    HERMES_CHECK_EQ(hits.front().message_id, std::string("1"));
}

HERMES_TEST(HeuristicJunkScorerWhitelistsKnownSendersAndFlagsSpam) {
    hermes::MemoryAddressBookService address_book;
    address_book.AddEntry({"Alice", "friend@example.com", ""});

    hermes::HeuristicJunkScorer scorer(&address_book);

    hermes::MessageRecord trusted;
    trusted.sender = "friend@example.com";
    trusted.subject = "URGENT but real";
    trusted.plain_text_body = "This is still allowed.";
    const auto trusted_verdict = scorer.Score(trusted);
    HERMES_CHECK(!trusted_verdict.is_junk);

    hermes::MessageRecord spam;
    spam.sender = "noreply@spam.example";
    spam.subject = "URGENT LOTTERY WINNER";
    spam.plain_text_body = "Click here for free money and viagra credit.";
    const auto spam_verdict = scorer.Score(spam);
    HERMES_CHECK(spam_verdict.is_junk);
    HERMES_CHECK(spam_verdict.score >= 10);
}
