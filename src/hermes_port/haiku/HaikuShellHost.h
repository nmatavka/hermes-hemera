#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hermes/DraftStore.h"
#include "hermes/IniSettingsStore.h"
#include "hermes/MailboxStore.h"
#include "hermes/MessageStore.h"
#include "hermes/PaigeRuntime.h"
#include "hermes/InMemoryWorkspaceModel.h"
#include "hermes/SignatureStore.h"
#include "hermes/ShellHost.h"
#include "hermes/StationeryStore.h"

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
    FilesystemDraftStore& Drafts();
    FilesystemMailboxStore& Mailboxes();
    FilesystemMessageStore& Messages();
    FilesystemStationeryStore& Stationery();
    FilesystemSignatureStore& Signatures();
    PaigeRuntime& Runtime();
    const std::optional<ComposeMessage>& PendingComposerMessage() const;

    void ShowMainWindow();
    void ReloadWorkspace();

private:
    void ShowComposeWindow(const ComposeMessage& message);
    void EnsureWorkspaceDirectories();
    std::filesystem::path DataRoot() const;

    std::unique_ptr<IniSettingsStore> settings_;
    std::unique_ptr<InMemoryWorkspaceModel> workspace_;
    std::unique_ptr<FilesystemDraftStore> draft_store_;
    std::unique_ptr<FilesystemMailboxStore> mailbox_store_;
    std::unique_ptr<FilesystemMessageStore> message_store_;
    std::unique_ptr<FilesystemStationeryStore> stationery_store_;
    std::unique_ptr<FilesystemSignatureStore> signature_store_;
    std::unique_ptr<PaigeRuntime> paige_runtime_;
    std::unique_ptr<HaikuMainWindow> main_window_;
    std::vector<std::unique_ptr<HaikuComposeWindow>> compose_windows_;
    std::optional<ComposeMessage> pending_composer_message_;
    std::string active_mailbox_id_;
};

}  // namespace hermes::haiku_port
