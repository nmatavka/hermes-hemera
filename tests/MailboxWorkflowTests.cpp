#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/FilterReportStore.h"
#include "hermes/FilterStore.h"
#include "hermes/JunkScorer.h"
#include "hermes/MailboxStore.h"
#include "hermes/MailboxWorkflow.h"
#include "hermes/MessageStore.h"

namespace {

hermes::MailboxRecord InboxMailbox() {
    return {"inbox", "Inbox", {}, "primary", hermes::MailboxProtocol::kLocal, "", false, true, 0};
}

hermes::MailboxRecord JunkMailbox() {
    return {"junk", "Junk", {}, "primary", hermes::MailboxProtocol::kLocal, "", false, true, 0};
}

hermes::MailboxRecord ArchiveMailbox() {
    return {"archive", "Archive", {}, "primary", hermes::MailboxProtocol::kLocal, "", false, false, 0};
}

hermes::MailboxRecord ProjectMailbox() {
    return {"project", "Project", {}, "primary", hermes::MailboxProtocol::kLocal, "", false, false, 0};
}

hermes::MailboxRecord FolderRecord() {
    hermes::MailboxRecord folder{"folder", "Folder", {}, "primary", hermes::MailboxProtocol::kLocal, "", false, false, 0};
    folder.kind = hermes::MailboxKind::kFolder;
    return folder;
}

hermes::MessageRecord BuildMessage(std::string id, std::string mailbox_id) {
    hermes::MessageRecord message;
    message.id = std::move(id);
    message.mailbox_id = std::move(mailbox_id);
    message.account_id = "primary";
    message.subject = "Subject";
    message.sender = "sender@example.com";
    message.recipients = "team@example.com";
    message.plain_text_body = "Plain body";
    return message;
}

}  // namespace

HERMES_TEST(MailboxWorkflowUpdatesLegacyStatusAndLabels) {
    hermes::tests::ScopedTempDirectory temp("hemera-mailbox-workflow-status");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    std::string error_message;
    HERMES_CHECK(mailbox_store.EnsureMailbox(InboxMailbox(), &error_message));

    auto message = BuildMessage("msg-1", "inbox");
    HERMES_CHECK(message_store.SaveMessage(message, &error_message));

    HERMES_CHECK(hermes::SetLegacyStatus(message_store,
                                         "inbox",
                                         {std::string("msg-1")},
                                         hermes::LegacyMessageStatus::kForwarded,
                                         &error_message));
    HERMES_CHECK(hermes::SetLabel(message_store, "inbox", {std::string("msg-1")}, 5, &error_message));

    const auto updated = message_store.GetMessage("inbox", "msg-1");
    HERMES_CHECK(static_cast<bool>(updated));
    HERMES_CHECK_EQ(updated->legacy_status, hermes::LegacyMessageStatus::kForwarded);
    HERMES_CHECK_EQ(updated->label_index, 5);
}

HERMES_TEST(MailboxWorkflowUpdatesPopServerStatusAndCoercesJunkFetchToDelete) {
    hermes::tests::ScopedTempDirectory temp("hemera-mailbox-workflow-pop-status");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    std::string error_message;
    HERMES_CHECK(mailbox_store.EnsureMailbox(InboxMailbox(), &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(JunkMailbox(), &error_message));

    auto inbox_message = BuildMessage("msg-1", "inbox");
    inbox_message.remote_id = "uidl-1";
    HERMES_CHECK(message_store.SaveMessage(inbox_message, &error_message));

    auto junk_message = BuildMessage("msg-2", "junk");
    junk_message.remote_id = "uidl-2";
    HERMES_CHECK(message_store.SaveMessage(junk_message, &error_message));

    HERMES_CHECK(hermes::SetPopServerStatus(message_store,
                                            InboxMailbox(),
                                            {std::string("msg-1")},
                                            hermes::PopServerStatus::kFetchDelete,
                                            false,
                                            &error_message));
    HERMES_CHECK(hermes::SetPopServerStatus(message_store,
                                            JunkMailbox(),
                                            {std::string("msg-2")},
                                            hermes::PopServerStatus::kFetch,
                                            true,
                                            &error_message));

    const auto inbox_updated = message_store.GetMessage("inbox", "msg-1");
    const auto junk_updated = message_store.GetMessage("junk", "msg-2");
    HERMES_CHECK(static_cast<bool>(inbox_updated));
    HERMES_CHECK(static_cast<bool>(junk_updated));
    HERMES_CHECK_EQ(inbox_updated->pop_server_status, hermes::PopServerStatus::kFetchDelete);
    HERMES_CHECK_EQ(junk_updated->pop_server_status, hermes::PopServerStatus::kDelete);
    HERMES_CHECK_EQ(hermes::EffectivePopServerStatus(JunkMailbox(), hermes::PopServerStatus::kFetch, true),
                    hermes::PopServerStatus::kDelete);
}

HERMES_TEST(MailboxWorkflowMarksLocalMessagesJunkAndMovesThemToJunkMailbox) {
    hermes::tests::ScopedTempDirectory temp("hemera-mailbox-workflow-junk");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::FilesystemFilterReportStore report_store;
    hermes::FilesystemFilterStore filter_store;
    hermes::HeuristicJunkScorer scorer;
    std::string error_message;

    HERMES_CHECK(mailbox_store.EnsureMailbox(InboxMailbox(), &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(JunkMailbox(), &error_message));

    auto message = BuildMessage("msg-1", "inbox");
    HERMES_CHECK(message_store.SaveMessage(message, &error_message));

    HERMES_CHECK(hermes::ApplyJunkActionToLocalMessages(message_store,
                                                        mailbox_store,
                                                        report_store,
                                                        InboxMailbox(),
                                                        {std::string("msg-1")},
                                                        hermes::MailboxJunkAction::kJunk,
                                                        scorer,
                                                        filter_store,
                                                        &error_message));

    HERMES_CHECK(!message_store.GetMessage("inbox", "msg-1"));
    const auto moved = message_store.GetMessage("junk", "msg-1");
    HERMES_CHECK(static_cast<bool>(moved));
    HERMES_CHECK_EQ(moved->junk_score, 100);
    HERMES_CHECK(moved->manually_junked);
}

HERMES_TEST(MailboxWorkflowAppliesFiltersToSelectedMessagesAndReportsMatches) {
    hermes::tests::ScopedTempDirectory temp("hemera-mailbox-workflow-filters");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    hermes::FilesystemMessageStore message_store(temp.Path());
    hermes::FilesystemFilterReportStore report_store;
    hermes::FilesystemFilterStore filter_store;
    std::string error_message;

    HERMES_CHECK(mailbox_store.EnsureMailbox(InboxMailbox(), &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox({"archive", "Archive", {}, "primary", hermes::MailboxProtocol::kLocal, "", false, false, 0},
                                             &error_message));

    auto message = BuildMessage("msg-1", "inbox");
    message.subject = "Project release";
    HERMES_CHECK(message_store.SaveMessage(message, &error_message));

    hermes::FilterRule rule;
    rule.name = "Release";
    rule.field = hermes::FilterField::kSubject;
    rule.operation = hermes::FilterOperation::kContains;
    rule.value = "release";
    rule.destination_mailbox = "archive";
    filter_store.AddOrReplace(rule);

    HERMES_CHECK(hermes::ApplyFiltersToMessages(message_store,
                                                mailbox_store,
                                                report_store,
                                                InboxMailbox(),
                                                {std::string("msg-1")},
                                                filter_store,
                                                &error_message));

    HERMES_CHECK(!message_store.GetMessage("inbox", "msg-1"));
    HERMES_CHECK(static_cast<bool>(message_store.GetMessage("archive", "msg-1")));
    const auto report = report_store.FindByMessageId("msg-1");
    HERMES_CHECK(static_cast<bool>(report));
    HERMES_CHECK_EQ(report->matched_rules.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(report->matched_rules.front(), std::string("Release"));
}

HERMES_TEST(MailboxWorkflowSuggestsManualFilterFromSharedSelectionValues) {
    auto first = BuildMessage("msg-1", "inbox");
    first.sender = "Alice <alice@example.com>";
    first.recipients = "team@example.com, dev@example.com";
    first.subject = "Re: Launch review";

    auto second = BuildMessage("msg-2", "inbox");
    second.sender = "Alice <alice@example.com>";
    second.recipients = "team@example.com, ops@example.com";
    second.subject = "Fwd: Launch review";

    const auto suggestion = hermes::SuggestManualFilter({first, second});
    HERMES_CHECK(static_cast<bool>(suggestion));
    HERMES_CHECK_EQ(suggestion->rule.field, hermes::FilterField::kFrom);
    HERMES_CHECK_EQ(suggestion->rule.value, std::string("alice@example.com"));
    HERMES_CHECK_EQ(suggestion->rule.destination_mailbox.value_or(""), std::string("alice@example.com"));
}

HERMES_TEST(MailboxWorkflowNormalizesRecentMailboxIdsAndSeedsInboxFallback) {
    hermes::tests::ScopedTempDirectory temp("hemera-mailbox-workflow-recents");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    std::string error_message;
    HERMES_CHECK(mailbox_store.EnsureMailbox(InboxMailbox(), &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(ArchiveMailbox(), &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(ProjectMailbox(), &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(FolderRecord(), &error_message));

    const auto normalized = hermes::NormalizeRecentMailboxIds(
        {"project", "", "missing", "archive", "project", "folder"}, mailbox_store, 2);
    HERMES_CHECK_EQ(normalized.size(), static_cast<std::size_t>(2));
    HERMES_CHECK_EQ(normalized[0], std::string("project"));
    HERMES_CHECK_EQ(normalized[1], std::string("archive"));

    const auto fallback = hermes::NormalizeRecentMailboxIds({"missing", "folder"}, mailbox_store, 5);
    HERMES_CHECK_EQ(fallback.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(fallback[0], std::string("inbox"));

    const auto disabled = hermes::NormalizeRecentMailboxIds({"project"}, mailbox_store, 0);
    HERMES_CHECK(disabled.empty());
}

HERMES_TEST(MailboxWorkflowRememberRecentMailboxMovesNewestToFrontAndPrunesStaleIds) {
    hermes::tests::ScopedTempDirectory temp("hemera-mailbox-workflow-remember-recents");
    hermes::FilesystemMailboxStore mailbox_store(temp.Path());
    std::string error_message;
    HERMES_CHECK(mailbox_store.EnsureMailbox(InboxMailbox(), &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(ArchiveMailbox(), &error_message));
    HERMES_CHECK(mailbox_store.EnsureMailbox(ProjectMailbox(), &error_message));

    const auto updated =
        hermes::RememberRecentMailboxId({"archive", "missing", "project"}, mailbox_store, "project", 2);
    HERMES_CHECK_EQ(updated.size(), static_cast<std::size_t>(2));
    HERMES_CHECK_EQ(updated[0], std::string("project"));
    HERMES_CHECK_EQ(updated[1], std::string("archive"));

    const auto unchanged =
        hermes::RememberRecentMailboxId({"archive"}, mailbox_store, "missing", 3);
    HERMES_CHECK_EQ(unchanged.size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(unchanged[0], std::string("archive"));
}
