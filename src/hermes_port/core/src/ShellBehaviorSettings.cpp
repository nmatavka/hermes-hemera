#include "hermes/ShellBehaviorSettings.h"

namespace hermes {

ShellBehaviorSettings ShellBehaviorSettingsFromSettings(const SettingsStore& settings,
                                                       std::string_view section) {
    ShellBehaviorSettings behavior;
    behavior.offline = settings.GetBool(section, "Offline", false);
    behavior.control_arrows = settings.GetBool(section, "ControlArrows", false);
    behavior.alt_arrows = settings.GetBool(section, "AltArrows", false);
    behavior.backspace_delete = settings.GetBool(section, "BackspaceDelete", true);
    behavior.reply_ctrl_r_to_all = settings.GetBool(section, "ReplyToAll", false);
    return behavior;
}

void ApplyShellBehaviorSettingsToSettings(const ShellBehaviorSettings& behavior,
                                          SettingsStore& settings,
                                          std::string_view section) {
    settings.SetString(section, "Offline", behavior.offline ? "1" : "0");
    settings.SetString(section, "ControlArrows", behavior.control_arrows ? "1" : "0");
    settings.SetString(section, "AltArrows", behavior.alt_arrows ? "1" : "0");
    settings.SetString(section, "BackspaceDelete", behavior.backspace_delete ? "1" : "0");
    settings.SetString(section, "ReplyToAll", behavior.reply_ctrl_r_to_all ? "1" : "0");
}

}  // namespace hermes
