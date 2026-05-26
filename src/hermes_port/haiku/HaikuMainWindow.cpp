#include "HaikuMainWindow.h"

#include <Application.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <StringItem.h>
#include <TextView.h>

#include "HaikuShellHost.h"
#include "hermes/WorkspaceModel.h"

namespace hermes::haiku_port {

HaikuMainWindow::HaikuMainWindow(HaikuShellHost& shell_host)
    : BWindow(BRect(100, 100, 1100, 800),
              "Hermes Hemera",
              B_TITLED_WINDOW,
              B_QUIT_ON_WINDOW_CLOSE),
      shell_host_(shell_host) {
    auto* menu_bar = new BMenuBar("menu-bar");
    auto* file_menu = new BMenu("File");
    file_menu->AddItem(new BMenuItem("New Message", nullptr));
    file_menu->AddSeparatorItem();
    file_menu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED)));
    menu_bar->AddItem(file_menu);

    mailbox_list_ = new BListView("mailboxes");
    message_list_ = new BListView("messages");
    preview_text_ = new BTextView("preview");
    preview_text_->MakeEditable(false);

    auto* top_split = new BSplitView(B_HORIZONTAL);
    top_split->AddChild(new BScrollView("mailboxes-scroll", mailbox_list_, 0, false, true));
    top_split->AddChild(new BScrollView("messages-scroll", message_list_, 0, false, true));

    auto* main_split = new BSplitView(B_VERTICAL);
    main_split->AddChild(top_split);
    main_split->AddChild(new BScrollView("preview-scroll", preview_text_, 0, true, true));

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(menu_bar)
        .Add(main_split);

    PopulateWorkspace();
}

bool HaikuMainWindow::QuitRequested() {
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}

void HaikuMainWindow::PopulateWorkspace() {
    mailbox_list_->MakeEmpty();
    message_list_->MakeEmpty();

    for (const auto& mailbox : shell_host_.Workspace().Mailboxes()) {
        mailbox_list_->AddItem(new BStringItem(mailbox.display_name.c_str()));
    }

    const auto inbox_messages = shell_host_.Workspace().MessagesForMailbox("inbox");
    for (const auto& message : inbox_messages) {
        message_list_->AddItem(new BStringItem(message.subject.c_str()));
    }

    preview_text_->SetText(
        "Hermes Hemera Haiku shell bootstrap\n\n"
        "This window is the native shell scaffold. The next migration steps hang "
        "Paige composition, WebKit display, and real workspace wiring off the "
        "portable interfaces in src/hermes_port/core.");
}

}  // namespace hermes::haiku_port
