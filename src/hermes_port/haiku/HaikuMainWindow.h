#pragma once

#include <string>
#include <vector>

#include <Window.h>

class BListView;
class BTextView;

namespace hermes::haiku_port {

class HaikuShellHost;

class HaikuMainWindow final : public BWindow {
public:
    explicit HaikuMainWindow(HaikuShellHost& shell_host);

    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;
    void RefreshWorkspace();

private:
    void PopulateWorkspace();
    void PopulateMessagesForCurrentMailbox();
    void PopulatePreview();

    HaikuShellHost& shell_host_;
    BListView* mailbox_list_ = nullptr;
    BListView* message_list_ = nullptr;
    BTextView* preview_text_ = nullptr;
    std::vector<std::string> mailbox_ids_;
    std::vector<std::string> message_ids_;
    std::string current_mailbox_id_ = "inbox";
    std::string current_message_id_;
};

}  // namespace hermes::haiku_port
