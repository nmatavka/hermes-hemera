#include "HaikuComposeWindow.h"

#include <Application.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

namespace hermes::haiku_port {

HaikuComposeWindow::HaikuComposeWindow(const ComposeMessage& message)
    : BWindow(BRect(140, 140, 1080, 860),
              "Compose Message",
              B_TITLED_WINDOW,
              B_ASYNCHRONOUS_CONTROLS) {
    auto* menu_bar = new BMenuBar("compose-menu-bar");
    auto* file_menu = new BMenu("File");
    file_menu->AddItem(new BMenuItem("Close", new BMessage(B_QUIT_REQUESTED)));
    menu_bar->AddItem(file_menu);

    auto* edit_menu = new BMenu("Edit");
    edit_menu->AddItem(new BMenuItem("Undo", nullptr));
    edit_menu->AddItem(new BMenuItem("Redo", nullptr));
    edit_menu->AddSeparatorItem();
    edit_menu->AddItem(new BMenuItem("Cut", nullptr));
    edit_menu->AddItem(new BMenuItem("Copy", nullptr));
    edit_menu->AddItem(new BMenuItem("Paste", nullptr));
    edit_menu->AddItem(new BMenuItem("Select All", nullptr));
    menu_bar->AddItem(edit_menu);

    auto* spelling_menu = new BMenu("Spelling");
    spelling_menu->AddItem(new BMenuItem("Check Document", nullptr));
    spelling_menu->AddItem(new BMenuItem("Ignore Word", nullptr));
    spelling_menu->AddItem(new BMenuItem("Add Word", nullptr));
    spelling_menu->AddItem(new BMenuItem("Replace Current", nullptr));
    menu_bar->AddItem(spelling_menu);

    to_control_ = new BTextControl("To", "", nullptr);
    cc_control_ = new BTextControl("Cc", "", nullptr);
    bcc_control_ = new BTextControl("Bcc", "", nullptr);
    subject_control_ = new BTextControl("Subject", "", nullptr);
    persona_control_ = new BTextControl("Persona", "", nullptr);
    reply_to_control_ = new BTextControl("Reply-To", "", nullptr);

    body_view_ = new BTextView("compose-body");
    body_view_->SetStylable(true);

    auto* paige_status =
        new BStringView("paige-status",
                        "Paige host scaffold: native Haiku device glue is the next editor integration step.");

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu_bar)
        .AddGroup(B_VERTICAL, 8)
            .SetInsets(B_USE_WINDOW_SPACING)
            .Add(to_control_)
            .Add(cc_control_)
            .Add(bcc_control_)
            .Add(subject_control_)
            .Add(persona_control_)
            .Add(reply_to_control_)
            .Add(paige_status)
            .Add(new BScrollView("compose-body-scroll", body_view_, 0, true, true))
        .End();

    PopulateMessage(message);
}

bool HaikuComposeWindow::QuitRequested() {
    Hide();
    return true;
}

void HaikuComposeWindow::PopulateMessage(const ComposeMessage& message) {
    to_control_->SetText(message.headers.to.c_str());
    cc_control_->SetText(message.headers.cc.c_str());
    bcc_control_->SetText(message.headers.bcc.c_str());
    subject_control_->SetText(message.headers.subject.c_str());
    persona_control_->SetText(message.headers.from_persona.c_str());
    reply_to_control_->SetText(message.headers.reply_to.c_str());
    body_view_->SetText(message.body.plain_text.c_str());
}

}  // namespace hermes::haiku_port
