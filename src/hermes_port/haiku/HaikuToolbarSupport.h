#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <Window.h>

#include <private/shared/ToolBar.h>

#include "hermes/ToolbarConfiguration.h"

class BHandler;

namespace hemera::haiku {

class HaikuShellHost;

constexpr uint32_t kToolbarOpenMailboxMessage = 'tbom';
constexpr uint32_t kToolbarComposeNicknameMessage = 'tbnk';
constexpr uint32_t kToolbarComposeStationeryMessage = 'tbst';
constexpr uint32_t kToolbarComposePersonaMessage = 'tbps';
constexpr uint32_t kToolbarRevealPluginMessage = 'tbpl';

struct ToolbarActionSpec {
    std::string id;
    std::string label;
    std::string tool_tip;
    std::string page;
    std::string group;
    std::string icon_id;
    std::string tool_id;
    std::string argument_key;
    std::string argument_value;
    uint32_t command = 0;
};

std::vector<ToolbarActionSpec> MainToolbarActionSpecs(HaikuShellHost& shell_host);
std::vector<ToolbarActionSpec> ComposeToolbarActionSpecs(HaikuShellHost& shell_host);
const std::vector<std::string>& MainToolbarDefaultEntries();
const std::vector<std::string>& ComposeToolbarDefaultEntries();
std::vector<std::string> ToolbarAllowedEntries(const std::vector<ToolbarActionSpec>& actions);
const ToolbarActionSpec* FindToolbarAction(const std::vector<ToolbarActionSpec>& actions,
                                          std::string_view action_id);

void PopulateToolbar(BToolBar& toolbar,
                     BHandler* target,
                     const std::vector<ToolbarActionSpec>& actions,
                     const hermes::ToolbarConfiguration& configuration,
                     bool show_tool_tips,
                     bool large_buttons);

class HaikuToolbarCustomizationWindow final : public BWindow {
public:
    using ApplyCallback =
        std::function<void(const hermes::ToolbarConfiguration&, bool show_tool_tips, bool large_buttons)>;

    HaikuToolbarCustomizationWindow(std::string title,
                                    std::vector<ToolbarActionSpec> actions,
                                    std::vector<std::string> default_entries,
                                    hermes::ToolbarConfiguration configuration,
                                    bool show_tool_tips,
                                    bool large_buttons,
                                    ApplyCallback apply_callback);

    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;

private:
    void PopulateGroups();
    void PopulateCurrentEntries();
    void PopulateAvailableEntries();
    void ApplyChanges();
    void SyncPreferenceCheckboxes();
    std::string SelectedAvailableActionId() const;
    int32_t SelectedCurrentIndex() const;

    std::vector<ToolbarActionSpec> actions_;
    std::vector<std::string> default_entries_;
    hermes::ToolbarConfiguration configuration_;
    bool show_tool_tips_ = true;
    bool large_buttons_ = false;
    ApplyCallback apply_callback_;
    class BMenuField* page_field_ = nullptr;
    class BMenuField* group_field_ = nullptr;
    class BCheckBox* tool_tips_box_ = nullptr;
    class BCheckBox* large_buttons_box_ = nullptr;
    class BListView* available_list_ = nullptr;
    class BListView* current_list_ = nullptr;
    std::vector<std::string> available_action_ids_;
    std::string active_page_;
    std::string active_group_;
};

}  // namespace hemera::haiku
