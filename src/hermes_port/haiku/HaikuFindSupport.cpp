#include "HaikuFindSupport.h"

#include <utility>

#include <Button.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <TextControl.h>

namespace hemera::haiku {

namespace {

std::string g_shared_find_query;

}  // namespace

bool HasSharedFindQuery() {
    return !g_shared_find_query.empty();
}

std::string SharedFindQuery() {
    return g_shared_find_query;
}

void SetSharedFindQuery(std::string_view query) {
    g_shared_find_query = std::string(query);
}

HaikuFindWindow::HaikuFindWindow(BMessenger target,
                                 uint32 confirm_what,
                                 uint32 closed_what,
                                 const char* title)
    : BWindow(BRect(220, 220, 520, 320),
              title,
              B_TITLED_WINDOW,
              B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
      target_(std::move(target)),
      confirm_what_(confirm_what),
      closed_what_(closed_what) {
    field_ = new BTextControl("shell-find-text", "Find", "", new BMessage(confirm_what_));
    auto* find_button = new BButton("shell-find-next", "Find Next", new BMessage(confirm_what_));
    find_button->MakeDefault(true);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
        .SetInsets(B_USE_DEFAULT_SPACING)
        .Add(field_)
        .AddGroup(B_HORIZONTAL, 8)
            .AddGlue()
            .Add(find_button)
        .End();
}

bool HaikuFindWindow::QuitRequested() {
    BMessage closed(closed_what_);
    target_.SendMessage(&closed);
    Hide();
    return false;
}

void HaikuFindWindow::MessageReceived(BMessage* message) {
    if (message != nullptr && message->what == confirm_what_) {
        BMessage command(confirm_what_);
        command.AddString("query", Query().c_str());
        target_.SendMessage(&command);
        return;
    }
    BWindow::MessageReceived(message);
}

void HaikuFindWindow::SetQuery(std::string_view query) {
    if (field_ != nullptr) {
        field_->SetText(std::string(query).c_str());
    }
}

std::string HaikuFindWindow::Query() const {
    return field_ != nullptr && field_->Text() != nullptr ? field_->Text() : "";
}

void HaikuFindWindow::FocusQuery() {
    if (field_ != nullptr && field_->TextView() != nullptr) {
        field_->TextView()->MakeFocus(true);
        field_->TextView()->SelectAll();
    }
}

}  // namespace hemera::haiku
