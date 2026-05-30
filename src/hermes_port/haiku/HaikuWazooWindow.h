#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Message.h>
#include <Window.h>

#include "hermes/GuiPreferences.h"

namespace hemera::haiku {

class HaikuShellHost;

class HaikuWazooWindow final : public BWindow {
public:
    struct ToolSpec {
        std::string id;
        std::string title;
    };

    HaikuWazooWindow(HaikuShellHost& shell_host,
                     std::string group_id,
                     std::string title,
                     std::vector<ToolSpec> tools,
                     const hermes::WazooWindowState& initial_state);

    bool QuitRequested() override;
    void MessageReceived(BMessage* message) override;
    void MenusBeginning() override;
    void WindowActivated(bool active) override;

    bool HasTool(std::string_view tool_id) const;
    bool ActivateTool(std::string_view tool_id);
    void Refresh(std::optional<std::string_view> tool_id = std::nullopt);
    const std::string& GroupId() const;
    hermes::WazooWindowState CurrentState() const;
    bool RequestTabSelection(int32 index);
    void HandleTabSelectionCommitted();

private:
    void ApplyActivePaneFocus();
    void UpdateMenuState();

    class BMenuBar* menu_bar_ = nullptr;
    class BMenuItem* print_preview_item_ = nullptr;
    class BMenuItem* print_one_item_ = nullptr;
    class BMenuItem* find_item_ = nullptr;
    class BMenuItem* find_again_item_ = nullptr;
    class BMenuItem* new_item_ = nullptr;
    class BMenuItem* duplicate_item_ = nullptr;
    class BMenuItem* save_item_ = nullptr;
    class BMenuItem* delete_item_ = nullptr;
    class BMenuItem* move_up_item_ = nullptr;
    class BMenuItem* move_down_item_ = nullptr;
    class BMenuItem* compose_item_ = nullptr;
    class BMenuItem* toggle_details_item_ = nullptr;
    class BTabView* tab_view_ = nullptr;
    HaikuShellHost& shell_host_;
    std::string group_id_;
    std::vector<ToolSpec> tools_;
    std::vector<class BView*> panes_;
    class BWindow* find_window_ = nullptr;
};

}  // namespace hemera::haiku
