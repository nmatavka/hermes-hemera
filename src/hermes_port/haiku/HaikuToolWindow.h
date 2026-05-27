#pragma once

#include <string>
#include <vector>

#include <Window.h>

class BButton;
class BListView;
class BStringView;
class BTextControl;
class BTextView;

namespace hermes::haiku_port {

class HaikuShellHost;

class HaikuToolWindow final : public BWindow {
public:
    HaikuToolWindow(HaikuShellHost& shell_host, std::string tool_id, std::string title);

    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;

    void Refresh();
    const std::string& ToolId() const;

private:
    struct ItemRecord {
        std::string id;
        std::string label;
        std::string detail;
    };

    void Populate();
    void PopulateMailboxes();
    void PopulateSignatures();
    void PopulateStationery();
    void PopulateNicknames();
    void PopulatePersonalities();
    void PopulateFilters();
    void PopulateFilterReport();
    void PopulateLinkHistory();
    void PopulateFileBrowser();
    void PopulateDirectoryResults();
    void SaveCurrent();
    void DeleteCurrent();
    void NewItem();
    std::string CurrentItemId() const;
    void UpdateDetailFromSelection();

    HaikuShellHost& shell_host_;
    std::string tool_id_;
    BStringView* summary_view_ = nullptr;
    BTextControl* name_control_ = nullptr;
    BTextControl* aux_control_ = nullptr;
    BListView* item_list_ = nullptr;
    BTextView* detail_view_ = nullptr;
    BButton* save_button_ = nullptr;
    BButton* delete_button_ = nullptr;
    BButton* action_button_ = nullptr;
    std::vector<ItemRecord> items_;
};

}  // namespace hermes::haiku_port
