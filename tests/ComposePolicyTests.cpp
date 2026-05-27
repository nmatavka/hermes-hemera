#include "TestPaths.h"
#include "TestRegistry.h"

#include "hermes/ComposeMessage.h"
#include "hermes/IniSettingsStore.h"

HERMES_TEST(ComposePolicyFromLegacySettingsMapsSignatureStationeryAndSendMode) {
    hermes::IniSettingsStore settings;
    std::string error_message;
    HERMES_CHECK(settings.LoadFromFile(hermes::tests::FixtureRoot() / "profile_snapshots" / "Eudora.box",
                                       &error_message));

    const hermes::ComposePolicy policy = hermes::ComposePolicyFromSettings(settings);
    HERMES_CHECK(policy.user_signatures_enabled);
    HERMES_CHECK_EQ(policy.default_signature_name, std::string(""));
    HERMES_CHECK_EQ(policy.default_stationery_name, std::string(""));
    HERMES_CHECK(policy.send_plain_and_styled);
    HERMES_CHECK(!policy.send_plain_only);
    HERMES_CHECK(!policy.warn_on_styled_send);
}

HERMES_TEST(ComposePolicyFromSettingsMapsMoodAndBossProtectorRuntimeFlags) {
    hermes::IniSettingsStore settings;
    settings.SetString("Settings", "DoMoodWatchCheck", "1");
    settings.SetString("Settings", "MoodWatchInterval", "1500");
    settings.SetString("Settings", "MoodWatchWarnWhenProbablyOffensive", "1");
    settings.SetString("Settings", "MoodWatchWarnFire", "1");
    settings.SetString("Settings", "BPWarnOutsideDomains", "1");
    settings.SetString("Settings", "BPOutsideDomains", "*.corp.example,team.example");
    settings.SetString("Settings", "BPAdditionalWarnDialog", "1");
    settings.SetString("Settings", "WarnQueueStyledText", "1");
    settings.SetString("Settings", "SendPlainAndStyled", "0");
    settings.SetString("Settings", "SendStyledOnly", "1");

    const hermes::ComposePolicy policy = hermes::ComposePolicyFromSettings(settings);
    HERMES_CHECK(policy.mood_watch_enabled);
    HERMES_CHECK_EQ(policy.mood_watch_interval_ms, 1500);
    HERMES_CHECK(policy.mood_warn_when_probably_offend);
    HERMES_CHECK(policy.mood_warn_when_on_fire);
    HERMES_CHECK(policy.boss_protector_warn_outside_domains);
    HERMES_CHECK_EQ(policy.boss_protector_outside_domains, std::string("*.corp.example,team.example"));
    HERMES_CHECK(policy.boss_protector_additional_warn_dialog);
    HERMES_CHECK(policy.warn_on_styled_send);
    HERMES_CHECK(policy.send_styled_only);
}

HERMES_TEST(ComposePolicyFromSettingsMapsLegacyComposeDefaults) {
    hermes::IniSettingsStore settings;
    settings.SetString("Settings", "SendBinHex", "1");
    settings.SetString("Settings", "UseQP", "0");
    settings.SetString("Settings", "WordWrap", "0");
    settings.SetString("Settings", "TabsInBody", "0");
    settings.SetString("Settings", "KeepCopies", "1");
    settings.SetString("Settings", "ReturnReceiptFlag", "1");
    settings.SetString("Settings", "WordWrapOnScreen", "1");
    settings.SetString("Settings", "WordWrapColumn", "72");
    settings.SetString("Settings", "WordWrapMax", "88");

    const hermes::ComposePolicy policy = hermes::ComposePolicyFromSettings(settings);
    HERMES_CHECK_EQ(policy.default_options.attachment_encoding, hermes::AttachmentEncodingMode::kBinHex);
    HERMES_CHECK(!policy.default_options.quoted_printable);
    HERMES_CHECK(!policy.default_options.word_wrap);
    HERMES_CHECK(!policy.default_options.tabs_in_body);
    HERMES_CHECK(policy.default_options.keep_copies);
    HERMES_CHECK(policy.return_receipt_legacy_header);
    HERMES_CHECK(policy.word_wrap_on_screen);
    HERMES_CHECK_EQ(policy.word_wrap_column, 72);
    HERMES_CHECK_EQ(policy.word_wrap_max, 88);
}
