#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/DraftStore.h"

HERMES_TEST(FilesystemDraftStoreRoundTripsComposeMessages) {
    hermes::tests::ScopedTempDirectory temp("hermes-drafts");
    hermes::FilesystemDraftStore store(temp.Path());

    hermes::ComposeMessage draft;
    draft.id = "draft-001";
    draft.headers.to = "dev@example.com";
    draft.headers.cc = "team@example.com";
    draft.headers.subject = "Quarterly plan";
    draft.headers.from_persona = "Work";
    draft.headers.reply_to = "reply@example.com";
    draft.policy.default_signature_name = "Standard";
    draft.policy.default_stationery_name = "FollowUp";
    draft.policy.mood_watch_enabled = true;
    draft.policy.boss_protector_warn_inside_domains = true;
    draft.policy.boss_protector_inside_domains = "corp.example";
    draft.body.plain_text = "Plain body";
    draft.body.html_fragment = "<p>Styled body</p>";
    draft.stationery_name = "FollowUp";
    draft.signature_name = "Standard";
    draft.managed_signature = {true, "Standard", 12, 8, "Signature"};

    std::string error_message;
    HERMES_CHECK(store.SaveDraft(draft, &error_message));

    const auto loaded = store.GetDraft("draft-001");
    HERMES_CHECK(static_cast<bool>(loaded));
    HERMES_CHECK_EQ(loaded->headers.subject, std::string("Quarterly plan"));
    HERMES_CHECK_EQ(loaded->headers.cc, std::string("team@example.com"));
    HERMES_CHECK_EQ(loaded->body.plain_text, std::string("Plain body"));
    HERMES_CHECK_EQ(loaded->body.html_fragment, std::string("<p>Styled body</p>"));
    HERMES_CHECK_EQ(loaded->policy.default_stationery_name, std::string("FollowUp"));
    HERMES_CHECK(loaded->policy.boss_protector_warn_inside_domains);
    HERMES_CHECK_EQ(loaded->policy.boss_protector_inside_domains, std::string("corp.example"));
    HERMES_CHECK(loaded->managed_signature.attached);
    HERMES_CHECK_EQ(loaded->managed_signature.name, std::string("Standard"));
    HERMES_CHECK_EQ(loaded->managed_signature.start, static_cast<std::size_t>(12));
    HERMES_CHECK_EQ(loaded->managed_signature.length, static_cast<std::size_t>(8));

    const auto drafts = store.ListDrafts();
    HERMES_CHECK_EQ(drafts.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(drafts.front().id, std::string("draft-001"));
}
