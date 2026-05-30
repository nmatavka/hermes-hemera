#include "hermes/MailboxUiSettings.h"

#include <algorithm>
#include <sstream>

namespace hermes {

namespace {

CtrlJMapping CtrlJMappingFromInt(int value) {
    switch (value) {
        case 1:
            return CtrlJMapping::kJunk;
        case 2:
            return CtrlJMapping::kFilter;
        default:
            return CtrlJMapping::kUnknown;
    }
}

int CtrlJMappingToInt(CtrlJMapping value) {
    switch (value) {
        case CtrlJMapping::kJunk:
            return 1;
        case CtrlJMapping::kFilter:
            return 2;
        case CtrlJMapping::kUnknown:
            break;
    }
    return 0;
}

std::optional<MailboxLabelDefinition> ParseLabelDefinition(int index, std::string_view value) {
    std::stringstream stream{std::string(value)};
    std::string red;
    std::string green;
    std::string blue;
    std::string name;
    if (!std::getline(stream, red, ',') || !std::getline(stream, green, ',') || !std::getline(stream, blue, ',') ||
        !std::getline(stream, name)) {
        return std::nullopt;
    }

    try {
        MailboxLabelDefinition definition;
        definition.index = index;
        definition.red = std::stoi(red);
        definition.green = std::stoi(green);
        definition.blue = std::stoi(blue);
        definition.name = name;
        return definition;
    } catch (...) {
        return std::nullopt;
    }
}

std::string SerializeLabelDefinition(const MailboxLabelDefinition& definition) {
    return std::to_string(definition.red) + "," + std::to_string(definition.green) + "," +
           std::to_string(definition.blue) + "," + definition.name;
}

}  // namespace

MailboxUiSettings MailboxUiSettingsFromSettings(const SettingsStore& settings,
                                               std::string_view section,
                                               std::string_view labels_section) {
    MailboxUiSettings mailbox_ui;
    mailbox_ui.ctrl_j_mapping = CtrlJMappingFromInt(settings.GetInt(section, "CtrlJMapping", 0));
    mailbox_ui.search_accel_switch =
        settings.GetBool(section, "SwitchFindKeyAccl", settings.GetBool(section, "SearchAccelSwitch", false));
    mailbox_ui.mailbox_show_junk = settings.GetBool(section, "MailboxShowJunk", false);
    mailbox_ui.mailbox_show_server_status = settings.GetBool(section, "MailboxShowServerStatus", true);
    mailbox_ui.mailbox_show_status = settings.GetBool(section, "MailboxShowStatus", true);
    mailbox_ui.mailbox_show_label = settings.GetBool(section, "MailboxShowLabel", true);
    mailbox_ui.mailbox_show_mood = settings.GetBool(section, "MailboxShowMood", true);
    mailbox_ui.always_enable_junk = settings.GetBool(section, "AlwaysEnableJunk", false);
    mailbox_ui.multiple_replies_for_multiple_selections =
        settings.GetBool(section, "MultipleRepliesForMultipleSelections", false) ||
        settings.GetBool(section, "MultipleRepliesForMultipleSelection", false);
    mailbox_ui.multiple_reply_warn_threshold =
        std::max(1, settings.GetInt(section, "ConConMultipleReplyWarnThreshold", 50));
    mailbox_ui.delete_fetched_junk = settings.GetBool(section, "DeleteFetchedJunk", true);
    mailbox_ui.show_mailbox_lines = settings.GetBool(section, "ShowMailboxLines", false);
    mailbox_ui.black_toc_lines = settings.GetBool(section, "BlackTocLines", false);
    mailbox_ui.whole_summary_label_color = settings.GetBool(section, "WholeSummaryLabelColor", true);
    mailbox_ui.comp_summary_italic = settings.GetBool(section, "CompSummaryItalic", true);

    const int label_count = std::max(0, settings.GetInt(labels_section, "LabelCount", 7));
    mailbox_ui.labels.reserve(static_cast<std::size_t>(label_count));
    for (int index = 1; index <= label_count; ++index) {
        const std::string key = "Label" + std::to_string(index);
        if (const auto value = settings.GetString(labels_section, key)) {
            if (const auto definition = ParseLabelDefinition(index, *value)) {
                mailbox_ui.labels.push_back(*definition);
            }
        }
    }
    return mailbox_ui;
}

void ApplyMailboxUiSettingsToSettings(const MailboxUiSettings& mailbox_ui,
                                      SettingsStore& settings,
                                      std::string_view section,
                                      std::string_view labels_section) {
    settings.SetString(section, "CtrlJMapping", std::to_string(CtrlJMappingToInt(mailbox_ui.ctrl_j_mapping)));
    settings.SetString(section, "SwitchFindKeyAccl", mailbox_ui.search_accel_switch ? "1" : "0");
    settings.SetString(section, "MailboxShowJunk", mailbox_ui.mailbox_show_junk ? "1" : "0");
    settings.SetString(section, "MailboxShowServerStatus", mailbox_ui.mailbox_show_server_status ? "1" : "0");
    settings.SetString(section, "MailboxShowStatus", mailbox_ui.mailbox_show_status ? "1" : "0");
    settings.SetString(section, "MailboxShowLabel", mailbox_ui.mailbox_show_label ? "1" : "0");
    settings.SetString(section, "MailboxShowMood", mailbox_ui.mailbox_show_mood ? "1" : "0");
    settings.SetString(section, "AlwaysEnableJunk", mailbox_ui.always_enable_junk ? "1" : "0");
    settings.SetString(section,
                       "MultipleRepliesForMultipleSelections",
                       mailbox_ui.multiple_replies_for_multiple_selections ? "1" : "0");
    settings.SetString(section,
                       "ConConMultipleReplyWarnThreshold",
                       std::to_string(std::max(1, mailbox_ui.multiple_reply_warn_threshold)));
    settings.SetString(section, "DeleteFetchedJunk", mailbox_ui.delete_fetched_junk ? "1" : "0");
    settings.SetString(section, "ShowMailboxLines", mailbox_ui.show_mailbox_lines ? "1" : "0");
    settings.SetString(section, "BlackTocLines", mailbox_ui.black_toc_lines ? "1" : "0");
    settings.SetString(section,
                       "WholeSummaryLabelColor",
                       mailbox_ui.whole_summary_label_color ? "1" : "0");
    settings.SetString(section, "CompSummaryItalic", mailbox_ui.comp_summary_italic ? "1" : "0");
    settings.SetString(labels_section, "LabelCount", std::to_string(mailbox_ui.labels.size()));
    for (const auto& label : mailbox_ui.labels) {
        settings.SetString(labels_section, "Label" + std::to_string(label.index), SerializeLabelDefinition(label));
    }
}

std::optional<MailboxLabelDefinition> FindMailboxLabelDefinition(const MailboxUiSettings& mailbox_ui, int index) {
    const auto it = std::find_if(mailbox_ui.labels.begin(), mailbox_ui.labels.end(), [&](const auto& label) {
        return label.index == index;
    });
    if (it == mailbox_ui.labels.end()) {
        return std::nullopt;
    }
    return *it;
}

std::string MailboxLabelName(const MailboxUiSettings& mailbox_ui, int index) {
    if (const auto label = FindMailboxLabelDefinition(mailbox_ui, index)) {
        return label->name;
    }
    return index <= 0 ? "No Label" : "Label " + std::to_string(index);
}

}  // namespace hermes
