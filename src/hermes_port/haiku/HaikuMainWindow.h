#pragma once

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

private:
    void PopulateWorkspace();

    HaikuShellHost& shell_host_;
    BListView* mailbox_list_ = nullptr;
    BListView* message_list_ = nullptr;
    BTextView* preview_text_ = nullptr;
};

}  // namespace hermes::haiku_port
