#include "TestRegistry.h"

#include "hermes/FilterEngine.h"

HERMES_TEST(RuleBasedFilterEngineAppliesMailboxAndReadActions) {
    hermes::RuleBasedFilterEngine engine;
    engine.AddRule({"boss mail",
                    hermes::FilterField::kFrom,
                    hermes::FilterOperation::kContains,
                    "boss@example.com",
                    std::string("Priority"),
                    true,
                    false,
                    true});

    hermes::MessageRecord message;
    message.sender = "Boss <boss@example.com>";
    message.subject = "Weekly status";

    const hermes::FilterResult result = engine.Evaluate(message);
    HERMES_CHECK(result.matched);
    HERMES_CHECK(result.mark_as_read);
    HERMES_CHECK(!result.mark_as_junk);
    HERMES_CHECK(static_cast<bool>(result.destination_mailbox));
    HERMES_CHECK_EQ(*result.destination_mailbox, std::string("Priority"));
    HERMES_CHECK_EQ(result.matched_rules.size(), static_cast<std::size_t>(1));
}

HERMES_TEST(RuleBasedFilterEngineCanAccumulateMultipleMatches) {
    hermes::RuleBasedFilterEngine engine;
    engine.AddRule({"newsletters",
                    hermes::FilterField::kSubject,
                    hermes::FilterOperation::kContains,
                    "newsletter",
                    std::nullopt,
                    false,
                    true,
                    false});
    engine.AddRule({"vip",
                    hermes::FilterField::kBody,
                    hermes::FilterOperation::kContains,
                    "travel policy",
                    std::string("HR"),
                    false,
                    false,
                    true});

    hermes::MessageRecord message;
    message.subject = "Monthly newsletter";
    message.plain_text_body = "Please review the travel policy update.";

    const hermes::FilterResult result = engine.Evaluate(message);
    HERMES_CHECK(result.matched);
    HERMES_CHECK(result.mark_as_junk);
    HERMES_CHECK(static_cast<bool>(result.destination_mailbox));
    HERMES_CHECK_EQ(*result.destination_mailbox, std::string("HR"));
    HERMES_CHECK_EQ(result.matched_rules.size(), static_cast<std::size_t>(2));
}
