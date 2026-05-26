#include "HaikuShellHost.h"

#include <Application.h>

#include "HaikuComposeWindow.h"
#include "HaikuMainWindow.h"

namespace hermes::haiku_port {

namespace {

class HermesApplication final : public BApplication {
public:
    explicit HermesApplication(HaikuShellHost& shell_host)
        : BApplication("application/x-vnd.hermes-hemera"),
          shell_host_(shell_host) {}

    void ReadyToRun() override {
        shell_host_.ShowMainWindow();
    }

private:
    HaikuShellHost& shell_host_;
};

}  // namespace

HaikuShellHost::HaikuShellHost()
    : settings_(std::make_unique<IniSettingsStore>()),
      workspace_(std::make_unique<InMemoryWorkspaceModel>()) {
    SeedWorkspace();
}

int HaikuShellHost::Run() {
    HermesApplication app(*this);
    app.Run();
    return 0;
}

bool HaikuShellHost::OpenMailbox(std::string_view mailbox_id) {
    active_mailbox_id_ = std::string(mailbox_id);
    return true;
}

bool HaikuShellHost::OpenComposer(const ComposeMessage& message) {
    if (!main_window_) {
        pending_composer_message_ = message;
        return true;
    }

    ShowComposeWindow(message);
    return true;
}

InMemoryWorkspaceModel& HaikuShellHost::Workspace() {
    return *workspace_;
}

IniSettingsStore& HaikuShellHost::Settings() {
    return *settings_;
}

const std::optional<ComposeMessage>& HaikuShellHost::PendingComposerMessage() const {
    return pending_composer_message_;
}

void HaikuShellHost::ShowMainWindow() {
    if (!main_window_) {
        main_window_ = std::make_unique<HaikuMainWindow>(*this);
    }

    main_window_->Show();

    if (pending_composer_message_) {
        ShowComposeWindow(*pending_composer_message_);
        pending_composer_message_.reset();
    }
}

void HaikuShellHost::ShowComposeWindow(const ComposeMessage& message) {
    auto compose_window = std::make_unique<HaikuComposeWindow>(message);
    compose_window->Show();
    compose_windows_.push_back(std::move(compose_window));
}

void HaikuShellHost::SeedWorkspace() {
    workspace_->AddMailbox({"inbox", "Inbox", 2});
    workspace_->AddMailbox({"out", "Out", 0});
    workspace_->AddMailbox({"drafts", "Drafts", 1});

    workspace_->AddMessage({
        "welcome-1",
        "inbox",
        "Welcome to the Haiku port",
        "Hermes Hemera",
        "Native shell, portable core, and renderer boundaries are now in place.",
        true,
    });
    workspace_->AddMessage({
        "welcome-2",
        "drafts",
        "Composer migration notes",
        "Hermes Hemera",
        "Paige remains the composition target while WebKit handles HTML message display.",
        true,
    });
}

}  // namespace hermes::haiku_port
