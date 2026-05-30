#include "TestRegistry.h"

#include "hermes/IniSettingsStore.h"
#include "hermes/MailboxUiSettings.h"

HERMES_TEST(MailboxUiSettingsLoadLegacyMailboxPreferencesAndLabels) {
    hermes::IniSettingsStore settings;
    settings.SetString("Settings", "CtrlJMapping", "2");
    settings.SetString("Settings", "SwitchFindKeyAccl", "1");
    settings.SetString("Settings", "MailboxShowJunk", "1");
    settings.SetString("Settings", "MailboxShowServerStatus", "0");
    settings.SetString("Settings", "MailboxShowStatus", "0");
    settings.SetString("Settings", "MailboxShowLabel", "0");
    settings.SetString("Settings", "MailboxShowMood", "0");
    settings.SetString("Settings", "AlwaysEnableJunk", "1");
    settings.SetString("Settings", "MultipleRepliesForMultipleSelection", "1");
    settings.SetString("Settings", "ConConMultipleReplyWarnThreshold", "75");
    settings.SetString("Settings", "DeleteFetchedJunk", "0");
    settings.SetString("Settings", "ShowMailboxLines", "1");
    settings.SetString("Settings", "BlackTocLines", "1");
    settings.SetString("Settings", "WholeSummaryLabelColor", "0");
    settings.SetString("Settings", "CompSummaryItalic", "0");
    settings.SetString("Labels", "LabelCount", "2");
    settings.SetString("Labels", "Label1", "255,128,0,Action");
    settings.SetString("Labels", "Label2", "0,128,255,Review");

    const auto mailbox_ui = hermes::MailboxUiSettingsFromSettings(settings);
    HERMES_CHECK_EQ(mailbox_ui.ctrl_j_mapping, hermes::CtrlJMapping::kFilter);
    HERMES_CHECK(mailbox_ui.search_accel_switch);
    HERMES_CHECK(mailbox_ui.mailbox_show_junk);
    HERMES_CHECK(!mailbox_ui.mailbox_show_server_status);
    HERMES_CHECK(!mailbox_ui.mailbox_show_status);
    HERMES_CHECK(!mailbox_ui.mailbox_show_label);
    HERMES_CHECK(!mailbox_ui.mailbox_show_mood);
    HERMES_CHECK(mailbox_ui.always_enable_junk);
    HERMES_CHECK(mailbox_ui.multiple_replies_for_multiple_selections);
    HERMES_CHECK_EQ(mailbox_ui.multiple_reply_warn_threshold, 75);
    HERMES_CHECK(!mailbox_ui.delete_fetched_junk);
    HERMES_CHECK(mailbox_ui.show_mailbox_lines);
    HERMES_CHECK(mailbox_ui.black_toc_lines);
    HERMES_CHECK(!mailbox_ui.whole_summary_label_color);
    HERMES_CHECK(!mailbox_ui.comp_summary_italic);
    HERMES_CHECK_EQ(mailbox_ui.labels.size(), static_cast<std::size_t>(2));
    HERMES_CHECK_EQ(mailbox_ui.labels[0].name, std::string("Action"));
    HERMES_CHECK_EQ(mailbox_ui.labels[1].blue, 255);
    HERMES_CHECK_EQ(hermes::MailboxLabelName(mailbox_ui, 2), std::string("Review"));
}

HERMES_TEST(MailboxUiSettingsRoundTripCtrlJAndLabelsBackToSettings) {
    hermes::MailboxUiSettings mailbox_ui;
    mailbox_ui.ctrl_j_mapping = hermes::CtrlJMapping::kJunk;
    mailbox_ui.search_accel_switch = true;
    mailbox_ui.mailbox_show_junk = true;
    mailbox_ui.mailbox_show_server_status = false;
    mailbox_ui.mailbox_show_status = false;
    mailbox_ui.mailbox_show_label = false;
    mailbox_ui.mailbox_show_mood = false;
    mailbox_ui.always_enable_junk = false;
    mailbox_ui.multiple_reply_warn_threshold = 60;
    mailbox_ui.delete_fetched_junk = false;
    mailbox_ui.show_mailbox_lines = true;
    mailbox_ui.black_toc_lines = true;
    mailbox_ui.whole_summary_label_color = false;
    mailbox_ui.comp_summary_italic = false;
    mailbox_ui.labels = {{1, 255, 128, 0, "Label One"}, {2, 0, 128, 255, "Label Two"}};

    hermes::IniSettingsStore settings;
    hermes::ApplyMailboxUiSettingsToSettings(mailbox_ui, settings);

    HERMES_CHECK_EQ(settings.GetString("Settings", "CtrlJMapping").value_or(""), std::string("1"));
    HERMES_CHECK_EQ(settings.GetString("Settings", "SwitchFindKeyAccl").value_or(""), std::string("1"));
    HERMES_CHECK_EQ(settings.GetString("Settings", "MailboxShowServerStatus").value_or(""), std::string("0"));
    HERMES_CHECK_EQ(settings.GetString("Settings", "DeleteFetchedJunk").value_or(""), std::string("0"));
    HERMES_CHECK_EQ(settings.GetString("Settings", "ShowMailboxLines").value_or(""), std::string("1"));
    HERMES_CHECK_EQ(settings.GetString("Settings", "BlackTocLines").value_or(""), std::string("1"));
    HERMES_CHECK_EQ(settings.GetString("Settings", "WholeSummaryLabelColor").value_or(""), std::string("0"));
    HERMES_CHECK_EQ(settings.GetString("Settings", "CompSummaryItalic").value_or(""), std::string("0"));
    HERMES_CHECK_EQ(settings.GetString("Labels", "Label1").value_or(""), std::string("255,128,0,Label One"));
    HERMES_CHECK_EQ(settings.GetString("Labels", "Label2").value_or(""), std::string("0,128,255,Label Two"));
}
