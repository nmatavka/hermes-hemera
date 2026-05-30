#include <Application.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "HaikuShellHost.h"
#include "hermes/ComposeMessage.h"

int main() {
    BApplication app("application/x-vnd.hemera-workflow-smoke");

    hemera::haiku::HaikuShellHost shell_host;
    shell_host.Settings().SetString("Settings", "MailboxPreviewPane", "1");
    shell_host.Settings().SetString("Settings", "SetPreviewRead", "1");
    shell_host.Settings().SetString("Settings", "SetPreviewReadSeconds", "1");
    shell_host.Settings().SetString("Settings", "TaskStatusBringToFront", "1");
    shell_host.Settings().SetString("Settings", "TaskErrorBringToFront", "1");
    shell_host.Settings().SetString("Settings", "HaikuUtilityPaneOpen", "1");
    shell_host.Settings().SetString("Settings", "HaikuUtilityPaneSelectedTab", "1");
    shell_host.ShowMainWindow();

    std::error_code ignored;
    const auto temp_root = std::filesystem::temp_directory_path() / "hemera-haiku-workflow-smoke";
    std::filesystem::create_directories(temp_root, ignored);
    const auto source_attachment = temp_root / "report.txt";
    {
        std::ofstream output(source_attachment);
        output << "haiku workflow attachment";
    }

    std::string error_message;
    if (!shell_host.Mailboxes().EnsureMailbox(
            {"imap:INBOX", "INBOX", {}, "imap", hermes::MailboxProtocol::kImap, "INBOX", true, false, 1},
            &error_message)) {
        return 1;
    }

    hermes::MessageRecord message;
    message.id = "smoke-imap-message";
    message.mailbox_id = "imap:INBOX";
    message.account_id = "imap";
    message.subject = "Workflow smoke";
    message.sender = "sender@example.com";
    message.recipients = "receiver@example.com";
    message.plain_text_body = "Attachment and IMAP workflow smoke body";
    message.html_body = "<p>Attachment and IMAP <strong>workflow</strong> smoke body</p>";
    message.styled_source = hermes::StyledDocumentSource::kHtml;
    message.styled_fidelity = hermes::StyledDocumentFidelity::kLossless;
    message.remote_id = "1";
    message.remote_mailbox = "INBOX";
    message.attachments.push_back({"report.txt",
                                   "text/plain",
                                   24,
                                   false,
                                   source_attachment.string(),
                                   "",
                                   "attachment",
                                   true,
                                   ""});
    if (!shell_host.Messages().SaveMessage(message, &error_message)) {
        return 1;
    }

    shell_host.ReloadWorkspace();
    if (!shell_host.OpenMessageWindow("imap:INBOX", "smoke-imap-message")) {
        return 1;
    }

    const auto saved_attachment = temp_root / "saved-report.txt";
    if (!shell_host.SaveAttachment("imap:INBOX", "smoke-imap-message", 0, saved_attachment)) {
        return 1;
    }
    if (!std::filesystem::exists(saved_attachment)) {
        return 1;
    }

    hermes::ImapActionRecord action;
    action.id = "smoke-action";
    action.kind = hermes::ImapActionKind::kFetchFullMessage;
    action.state = hermes::ImapActionState::kFailed;
    action.account_id = "imap";
    action.mailbox_id = "imap:INBOX";
    action.remote_mailbox = "INBOX";
    action.message_id = "smoke-imap-message";
    action.remote_message_id = "1";
    action.last_error = "temporary failure";
    if (!shell_host.ImapActions().SaveAction(action, &error_message)) {
        return 1;
    }

    if (!shell_host.RetryTask("smoke-action")) {
        return 1;
    }
    const auto retried = shell_host.ImapActions().GetAction("smoke-action");
    if (!retried || retried->state != hermes::ImapActionState::kPending) {
        return 1;
    }

    if (!shell_host.CancelTask("smoke-action")) {
        return 1;
    }
    const auto cancelled = shell_host.ImapActions().GetAction("smoke-action");
    if (!cancelled || cancelled->state != hermes::ImapActionState::kCancelled) {
        return 1;
    }

    hermes::ComposeMessage compose;
    compose.id = "workflow-smoke-compose";
    compose.headers.subject = "Compose attachment smoke";
    compose.body.plain_text = "Compose body";
    compose.attachments.push_back({"report.txt", source_attachment, "text/plain", 24, "", false});
    if (!shell_host.OpenComposer(compose)) {
        return 1;
    }

    shell_host.ReloadWorkspace();
    return 0;
}
