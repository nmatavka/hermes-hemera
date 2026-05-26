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
