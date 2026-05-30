#include "hermes/SelectedTextUrlSettings.h"

#include <cctype>

namespace hermes {

namespace {

constexpr int kSelectedTextUrlSlotCount = 7;

std::string UrlEncode(std::string_view value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size() * 3);
    for (unsigned char ch : value) {
        const bool unreserved =
            std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~';
        if (unreserved) {
            encoded.push_back(static_cast<char>(ch));
            continue;
        }
        if (ch == ' ') {
            encoded.push_back('+');
            continue;
        }
        encoded.push_back('%');
        encoded.push_back(kHex[(ch >> 4) & 0x0F]);
        encoded.push_back(kHex[ch & 0x0F]);
    }
    return encoded;
}

void ReplaceAll(std::string* text, std::string_view needle, std::string_view replacement) {
    if (text == nullptr || needle.empty()) {
        return;
    }
    std::size_t offset = 0;
    while ((offset = text->find(needle, offset)) != std::string::npos) {
        text->replace(offset, needle.size(), replacement);
        offset += replacement.size();
    }
}

}  // namespace

std::vector<SelectedTextUrlAction> SelectedTextUrlActionsFromSettings(
    const SettingsStore& settings,
    std::string_view section) {
    std::vector<SelectedTextUrlAction> actions;
    for (int slot = 1; slot <= kSelectedTextUrlSlotCount; ++slot) {
        const std::string key = "SelectedTextURL" + std::to_string(slot);
        const auto value = settings.GetString(section, key);
        if (!value || value->empty()) {
            continue;
        }
        const std::size_t tab = value->find('\t');
        if (tab == std::string::npos) {
            continue;
        }
        SelectedTextUrlAction action;
        action.slot = slot;
        action.label = value->substr(0, tab);
        action.url_format = value->substr(tab + 1);
        if (action.label.empty() || action.url_format.empty()) {
            continue;
        }
        actions.push_back(std::move(action));
    }
    return actions;
}

std::optional<int> SelectedTextUrlAcceleratorDigitForSlot(int slot) {
    switch (slot) {
        case 1:
            return 7;
        case 2:
            return 8;
        case 3:
            return 9;
        case 4:
            return 2;
        case 5:
            return 3;
        case 6:
            return 4;
        case 7:
            return 5;
        default:
            return std::nullopt;
    }
}

std::string SelectedTextUrlAcceleratorLabelForSlot(int slot) {
    const auto digit = SelectedTextUrlAcceleratorDigitForSlot(slot);
    if (!digit) {
        return {};
    }
    return "Cmd+" + std::to_string(*digit);
}

std::string BuildSelectedTextUrl(const SelectedTextUrlAction& action, std::string_view selected_text) {
    const std::string encoded = UrlEncode(selected_text);
    std::string built = action.url_format;
    const std::string original = std::string(selected_text);

    ReplaceAll(&built, "%s", encoded);
    ReplaceAll(&built, "%1", encoded);
    ReplaceAll(&built, "^0", encoded);
    ReplaceAll(&built, "{selection}", encoded);
    ReplaceAll(&built, "{selection_raw}", original);

    if (built == action.url_format) {
        built += encoded;
    }
    return built;
}

}  // namespace hermes
