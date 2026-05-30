#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hermes/SettingsStore.h"

namespace hermes {

struct SelectedTextUrlAction {
    int slot = 0;
    std::string label;
    std::string url_format;
};

std::vector<SelectedTextUrlAction> SelectedTextUrlActionsFromSettings(
    const SettingsStore& settings,
    std::string_view section = "Settings");
std::optional<int> SelectedTextUrlAcceleratorDigitForSlot(int slot);
std::string SelectedTextUrlAcceleratorLabelForSlot(int slot);
std::string BuildSelectedTextUrl(const SelectedTextUrlAction& action, std::string_view selected_text);

}  // namespace hermes
