#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/ComposeController.h"
#include "hermes/MemoryRichTextSurface.h"
#include "hermes/NicknameStore.h"
#include "hermes/SignatureStore.h"

#include <fstream>

namespace {

class FakeSpellService final : public hermes::SpellService {
public:
    bool IsAvailable() const override {
        return true;
    }

    std::string EngineName() const override {
        return "fake-spell";
    }

    std::vector<hermes::SpellIssue> Check(std::string_view text,
                                          const hermes::SpellCheckRequest& /*request*/) override {
        std::vector<hermes::SpellIssue> issues;
        std::string haystack(text);

        const auto add_issue = [&](std::string_view needle) {
            const std::size_t position = haystack.find(needle);
            if (position != std::string::npos) {
                issues.push_back({position, needle.size(), std::string(needle)});
            }
        };

        add_issue("teh");
        add_issue("mispell");
        return issues;
    }

    std::vector<std::string> Suggest(std::string_view word,
                                     const hermes::SpellCheckRequest& /*request*/) override {
        if (word == "teh") {
            return {"the"};
        }
        if (word == "mispell") {
            return {"misspell"};
        }
        return {};
    }

    bool AddWordToUserDictionary(std::string_view word,
                                 const hermes::SpellCheckRequest& /*request*/) override {
        added_words.push_back(std::string(word));
        return true;
    }

    void IgnoreWord(std::string_view word) override {
        ignored_words.push_back(std::string(word));
    }

    std::vector<std::string> added_words;
    std::vector<std::string> ignored_words;
};

class FakeMoodWatchAnalyzer final : public hermes::MoodWatchAnalyzer {
public:
    bool IsAvailable() const override {
        return true;
    }

    hermes::MoodWatchResult Analyze(std::string_view text,
                                    const hermes::MoodWatchOptions& /*options*/) override {
        hermes::MoodWatchResult result;
        result.available = true;
        result.score = 3;

        const std::string haystack(text);
        const std::size_t position = haystack.find("flame");
        if (position != std::string::npos) {
            result.matches.push_back({position, 5, 2});
        }
        return result;
    }
};

}  // namespace

HERMES_TEST(ComposeControllerAppliesDefaultStationeryAndSupportsEditCommands) {
    hermes::MemoryRichTextSurface surface;
    hermes::FilesystemStationeryStore stationery_store;
    hermes::FilesystemSignatureStore signature_store;
    std::string error_message;
    HERMES_CHECK(stationery_store.Discover(hermes::tests::FixtureRoot() / "compose" / "stationery",
                                           &error_message));
    HERMES_CHECK(signature_store.Discover(hermes::tests::FixtureRoot() / "compose" / "signatures",
                                          &error_message));

    hermes::ComposeMessage message;
    message.id = "compose-1";
    message.policy.default_stationery_name = "FollowUp";

    hermes::ComposeController controller(
        surface, nullptr, nullptr, nullptr, &stationery_store, &signature_store);
    HERMES_CHECK(controller.Load(message));
    HERMES_CHECK_EQ(controller.HeaderValue(hermes::ComposeHeaderField::kSubject), std::string("Follow-up"));
    HERMES_CHECK_EQ(controller.Snapshot().headers.from_persona, std::string("Work"));
    HERMES_CHECK_EQ(controller.Snapshot().signature_name, std::string("Standard"));
    HERMES_CHECK(controller.Snapshot().body.plain_text.find("before Friday") != std::string::npos);
    HERMES_CHECK(controller.Snapshot().body.plain_text.find("Nick Example") != std::string::npos);
    HERMES_CHECK(controller.Snapshot().managed_signature.attached);
    HERMES_CHECK(!controller.IsDirty());

    HERMES_CHECK(controller.SelectAll());
    const std::string copied = controller.CopySelection();
    HERMES_CHECK(!copied.empty());
    const std::string cut = controller.CutSelection();
    HERMES_CHECK_EQ(cut, copied);
    HERMES_CHECK(controller.PasteText("Replacement body"));
    HERMES_CHECK(controller.Undo());
    HERMES_CHECK(controller.Redo());
    HERMES_CHECK_EQ(controller.Snapshot().body.plain_text, std::string("Replacement body"));
}

HERMES_TEST(ComposeControllerRoutesSpellMoodBossProtectorAndSendValidation) {
    hermes::MemoryRichTextSurface surface;
    FakeSpellService spell_service;
    FakeMoodWatchAnalyzer mood_watch;
    hermes::FlatFileNicknameStore nicknames;
    nicknames.AddOrReplace({"bosses",
                            "Bosses",
                            {"ceo@corp.example"},
                            "Sensitive list",
                            false,
                            true});

    hermes::ComposeMessage message;
    message.id = "compose-2";
    message.headers.to = "bosses";
    message.headers.subject = "teh flame update";
    message.body.plain_text = "This body has a mispell and another flame marker.";
    message.body.html_fragment = "<p>This body has a mispell and another flame marker.</p>";
    message.body.styled_source = hermes::StyledDocumentSource::kHtml;
    message.policy.warn_on_styled_send = true;
    message.policy.send_plain_and_styled = true;
    message.policy.boss_protector_additional_warn_dialog = true;
    message.policy.mood_warn_when_probably_offend = true;

    hermes::ComposeController controller(surface, &spell_service, &mood_watch, &nicknames, nullptr);
    HERMES_CHECK(controller.Load(message));
    HERMES_CHECK(controller.CheckDocument());
    HERMES_CHECK_EQ(controller.SpellIssues().size(), static_cast<std::size_t>(2));
    HERMES_CHECK(!surface.Diagnostics().empty());
    HERMES_CHECK(static_cast<bool>(controller.StatusBanner()));
    const auto suggestions = controller.SuggestionsForCurrentIssue();
    HERMES_CHECK_EQ(suggestions.front(), std::string("the"));
    HERMES_CHECK(controller.ReplaceCurrent("the"));
    HERMES_CHECK_EQ(controller.HeaderValue(hermes::ComposeHeaderField::kSubject),
                    std::string("the flame update"));

    controller.MarkBodyEdited();
    const auto checks = controller.ServiceAutomaticChecks(std::chrono::milliseconds(1500));
    HERMES_CHECK(checks.spell_checked);
    HERMES_CHECK(checks.mood_checked);
    HERMES_CHECK(checks.boss_protector_checked);

    const auto boss_result = controller.RunBossProtector();
    HERMES_CHECK(boss_result.warning_required);
    HERMES_CHECK(!boss_result.hits.empty());

    const auto validation = controller.ValidateForSend();
    HERMES_CHECK(validation.allowed_to_send);
    HERMES_CHECK(validation.styled_send.has_styles);
    HERMES_CHECK(validation.styled_send.should_warn);
    HERMES_CHECK(validation.mood_watch.score >= 3);
    HERMES_CHECK(validation.warnings.size() >= static_cast<std::size_t>(3));
    HERMES_CHECK(static_cast<bool>(controller.StatusBanner()));
    HERMES_CHECK(controller.Diagnostics().size() >= static_cast<std::size_t>(3));
}

HERMES_TEST(ComposeControllerReplacesAndDetachesManagedSignatures) {
    hermes::MemoryRichTextSurface surface;
    hermes::FilesystemSignatureStore signature_store;
    std::string error_message;
    HERMES_CHECK(signature_store.Discover(hermes::tests::FixtureRoot() / "compose" / "signatures",
                                          &error_message));

    hermes::ComposeMessage message;
    message.id = "compose-3";
    message.signature_name = "Standard";
    message.body.plain_text = "Body";

    hermes::ComposeController controller(surface, nullptr, nullptr, nullptr, nullptr, &signature_store);
    HERMES_CHECK(controller.Load(message));
    HERMES_CHECK(controller.Snapshot().managed_signature.attached);
    HERMES_CHECK(controller.Snapshot().body.plain_text.find("Nick Example") != std::string::npos);

    HERMES_CHECK(controller.ApplySignature("Alternate"));
    HERMES_CHECK(controller.Snapshot().body.plain_text.find("Status Desk") != std::string::npos);
    HERMES_CHECK(controller.Snapshot().body.html_fragment.find("<strong>") != std::string::npos);

    const auto managed = controller.Snapshot().managed_signature;
    HERMES_CHECK(managed.attached);
    HERMES_CHECK(surface.SetSelection({managed.start + 2, 0}));
    HERMES_CHECK(controller.PasteText("x"));
    HERMES_CHECK(!controller.Snapshot().managed_signature.attached);
}

HERMES_TEST(ComposeControllerAddsAndRemovesAttachments) {
    hermes::tests::ScopedTempDirectory temp("hemera-compose-attachments");
    const auto source_attachment = temp.Path() / "brief.txt";
    {
        std::ofstream output(source_attachment);
        output << "brief attachment";
    }

    hermes::MemoryRichTextSurface surface;
    hermes::ComposeMessage message;
    message.id = "compose-4";
    message.body.plain_text = "Attachment test";

    hermes::ComposeController controller(surface, nullptr, nullptr, nullptr, nullptr, nullptr);
    HERMES_CHECK(controller.Load(message));
    HERMES_CHECK(controller.AddAttachment({"brief.txt",
                                          source_attachment,
                                          "text/plain",
                                          0,
                                          "cid-brief",
                                          false}));
    HERMES_CHECK_EQ(controller.Attachments().size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(controller.Attachments().front().display_name, std::string("brief.txt"));
    HERMES_CHECK(controller.Attachments().front().size > 0);
    HERMES_CHECK(controller.RemoveAttachment(0));
    HERMES_CHECK(controller.Attachments().empty());
}
