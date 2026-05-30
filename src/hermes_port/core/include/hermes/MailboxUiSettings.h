#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/SettingsStore.h"

namespace hermes {

enum class CtrlJMapping {
    kUnknown = 0,
    kJunk = 1,
    kFilter = 2,
};

struct MailboxLabelDefinition {
    int index = 0;
    int red = 0;
    int green = 0;
    int blue = 0;
    std::string name;
};

struct MailboxUiSettings {
    CtrlJMapping ctrl_j_mapping = CtrlJMapping::kUnknown;
    bool search_accel_switch = false;
    bool mailbox_show_junk = false;
    bool mailbox_show_server_status = true;
    bool mailbox_show_status = true;
    bool mailbox_show_label = true;
    bool mailbox_show_mood = true;
    bool always_enable_junk = false;
    bool multiple_replies_for_multiple_selections = false;
    int multiple_reply_warn_threshold = 50;
    bool delete_fetched_junk = true;
    bool show_mailbox_lines = false;
    bool black_toc_lines = false;
    bool whole_summary_label_color = true;
    bool comp_summary_italic = true;
    std::vector<MailboxLabelDefinition> labels;
};

MailboxUiSettings MailboxUiSettingsFromSettings(const SettingsStore& settings,
                                               std::string_view section = "Settings",
                                               std::string_view labels_section = "Labels");
void ApplyMailboxUiSettingsToSettings(const MailboxUiSettings& mailbox_ui,
                                      SettingsStore& settings,
                                      std::string_view section = "Settings",
                                      std::string_view labels_section = "Labels");
std::optional<MailboxLabelDefinition> FindMailboxLabelDefinition(const MailboxUiSettings& mailbox_ui, int index);
std::string MailboxLabelName(const MailboxUiSettings& mailbox_ui, int index);

}  // namespace hermes
