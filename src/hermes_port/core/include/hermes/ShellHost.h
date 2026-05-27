#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include "hermes/ComposeMessage.h"

namespace hermes {

class ShellHost {
public:
    virtual ~ShellHost() = default;

    virtual int Run() = 0;
    virtual bool OpenMailbox(std::string_view mailbox_id) = 0;
    virtual bool OpenComposer(const ComposeMessage& message) = 0;
    virtual bool SendQueued() = 0;
    virtual bool CheckMail() = 0;
    virtual bool SendAndReceive() = 0;
    virtual bool RefreshMailbox(std::string_view mailbox_id) = 0;
    virtual bool ResyncMailbox(std::string_view mailbox_id) = 0;
    virtual bool DeleteMessage(std::string_view mailbox_id, std::string_view message_id) = 0;
    virtual bool UndeleteMessage(std::string_view mailbox_id, std::string_view message_id) = 0;
    virtual bool MoveMessage(std::string_view mailbox_id,
                             std::string_view message_id,
                             std::string_view destination_mailbox_id) = 0;
    virtual bool CopyMessage(std::string_view mailbox_id,
                             std::string_view message_id,
                             std::string_view destination_mailbox_id) = 0;
    virtual bool CreateRemoteMailbox(std::string_view account_id, std::string_view remote_name) = 0;
    virtual bool RenameRemoteMailbox(std::string_view mailbox_id, std::string_view new_remote_name) = 0;
    virtual bool DeleteRemoteMailbox(std::string_view mailbox_id) = 0;
    virtual std::optional<std::filesystem::path> AttachmentPath(std::string_view mailbox_id,
                                                                std::string_view message_id,
                                                                std::size_t attachment_index) const = 0;
    virtual bool SaveAttachment(std::string_view mailbox_id,
                                std::string_view message_id,
                                std::size_t attachment_index,
                                const std::filesystem::path& destination_path) = 0;
    virtual bool SaveAllAttachments(std::string_view mailbox_id,
                                    std::string_view message_id,
                                    const std::filesystem::path& destination_directory) = 0;
    virtual bool FetchAttachment(std::string_view mailbox_id,
                                 std::string_view message_id,
                                 std::size_t attachment_index) = 0;
    virtual bool FetchFullMessage(std::string_view mailbox_id, std::string_view message_id) = 0;
    virtual bool RetryTask(std::string_view task_or_action_id) = 0;
    virtual bool CancelTask(std::string_view task_or_action_id) = 0;
    virtual bool StopActiveTasks() = 0;
};

}  // namespace hermes
