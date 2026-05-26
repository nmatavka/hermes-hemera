#pragma once

#include <memory>
#include <optional>
#include <string>

#include "hermes/IniSettingsStore.h"
#include "hermes/InMemoryWorkspaceModel.h"
#include "hermes/ShellHost.h"

namespace hermes::haiku_port {

class HaikuMainWindow;

class HaikuShellHost final : public ShellHost {
public:
    HaikuShellHost();

    int Run() override;
    bool OpenMailbox(std::string_view mailbox_id) override;
    bool OpenComposer(const RichTextDocument& document) override;

    InMemoryWorkspaceModel& Workspace();
    IniSettingsStore& Settings();
    const std::optional<RichTextDocument>& PendingComposerDocument() const;

    void ShowMainWindow();

private:
    void SeedWorkspace();

    std::unique_ptr<IniSettingsStore> settings_;
    std::unique_ptr<InMemoryWorkspaceModel> workspace_;
    std::unique_ptr<HaikuMainWindow> main_window_;
    std::optional<RichTextDocument> pending_composer_document_;
    std::string active_mailbox_id_;
};

}  // namespace hermes::haiku_port
