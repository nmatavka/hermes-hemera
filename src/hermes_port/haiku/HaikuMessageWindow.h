#pragma once

#include <optional>
#include <string>
#include <vector>

#include <Window.h>

#include "hermes/MessageRenderer.h"

class BListView;
class BStringView;
class BTextView;
class BView;

namespace hermes {
struct MessageDetail;
}

namespace hemera::haiku {

class HaikuShellHost;
class HaikuWebKitMessageView;

class HaikuMessageWindow final : public BWindow {
public:
    HaikuMessageWindow(HaikuShellHost& shell_host, std::string mailbox_id, std::string message_id);
    ~HaikuMessageWindow() override;

    void MessageReceived(BMessage* message) override;
    bool QuitRequested() override;

    bool MatchesMessage(std::string_view mailbox_id, std::string_view message_id) const;
    bool LoadMessage(std::string mailbox_id, std::string message_id);
    void RefreshFromWorkspace();

private:
    std::optional<hermes::MessageRenderRequest> BuildRenderRequest(const hermes::MessageDetail& detail) const;
    void PopulateHeader(const hermes::MessageDetail& detail);
    void PopulateBody(const hermes::MessageDetail& detail);
    void PopulateAttachments(const hermes::MessageDetail* detail);
    void ShowAttachmentContextMenu(BPoint where);
    void HandleOpenSelectedAttachment();
    void HandleSaveSelectedAttachment();
    void HandleFetchSelectedAttachment();

    HaikuShellHost& shell_host_;
    std::string mailbox_id_;
    std::string message_id_;
    std::string render_notice_;
    BStringView* status_view_ = nullptr;
    BStringView* subject_view_ = nullptr;
    BStringView* from_view_ = nullptr;
    BStringView* to_view_ = nullptr;
    BStringView* date_view_ = nullptr;
    BStringView* state_view_ = nullptr;
    BView* plain_root_ = nullptr;
    BTextView* plain_text_ = nullptr;
    HaikuWebKitMessageView* web_view_ = nullptr;
    BView* attachment_container_ = nullptr;
    BListView* attachment_list_ = nullptr;
    std::vector<std::size_t> attachment_indices_;
};

}  // namespace hemera::haiku
