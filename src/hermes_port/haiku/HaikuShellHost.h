#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hermes/IniSettingsStore.h"
#include "hermes/InMemoryWorkspaceModel.h"
#include "hermes/ShellHost.h"

namespace hermes::haiku_port {

class HaikuMainWindow;
class HaikuComposeWindow;

class HaikuShellHost final : public ShellHost {
public:
    HaikuShellHost();

    int Run() override;
    bool OpenMailbox(std::string_view mailbox_id) override;
    bool OpenComposer(const ComposeMessage& message) override;

    InMemoryWorkspaceModel& Workspace();
    IniSettingsStore& Settings();
    const std::optional<ComposeMessage>& PendingComposerMessage() const;

    void ShowMainWindow();

private:
    void ShowComposeWindow(const ComposeMessage& message);
    void SeedWorkspace();

    std::unique_ptr<IniSettingsStore> settings_;
    std::unique_ptr<InMemoryWorkspaceModel> workspace_;
    std::unique_ptr<HaikuMainWindow> main_window_;
    std::vector<std::unique_ptr<HaikuComposeWindow>> compose_windows_;
    std::optional<ComposeMessage> pending_composer_message_;
    std::string active_mailbox_id_;
};

}  // namespace hermes::haiku_port
