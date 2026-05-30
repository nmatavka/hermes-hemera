#pragma once

#include <string_view>

#include "hermes/SettingsStore.h"

namespace hermes {

struct ShellBehaviorSettings {
    bool offline = false;
    bool control_arrows = false;
    bool alt_arrows = false;
    bool backspace_delete = true;
    bool reply_ctrl_r_to_all = false;
};

ShellBehaviorSettings ShellBehaviorSettingsFromSettings(const SettingsStore& settings,
                                                       std::string_view section = "Settings");
void ApplyShellBehaviorSettingsToSettings(const ShellBehaviorSettings& behavior,
                                          SettingsStore& settings,
                                          std::string_view section = "Settings");

}  // namespace hermes
