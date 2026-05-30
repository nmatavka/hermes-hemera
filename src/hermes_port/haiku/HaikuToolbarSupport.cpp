#include "HaikuToolbarSupport.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

#include <Button.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Message.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>

#include "HaikuIconCatalog.h"
#include "HaikuShellHost.h"

namespace hemera::haiku {

namespace {

constexpr uint32_t kGroupChangedMessage = 'tgrc';
constexpr uint32_t kAddMessage = 'tbad';
constexpr uint32_t kRemoveMessage = 'tbde';
constexpr uint32_t kMoveUpMessage = 'tbup';
constexpr uint32_t kMoveDownMessage = 'tbdn';
constexpr uint32_t kAddSeparatorMessage = 'tbsp';
constexpr uint32_t kResetMessage = 'tbrs';
constexpr uint32_t kApplyMessage = 'tbap';
constexpr uint32_t kPageChangedMessage = 'tpgc';

ToolbarActionSpec MakeAction(std::string id,
                             std::string label,
                             std::string tool_tip,
                             std::string page,
                             std::string group,
                             uint32_t command,
                             std::string icon_id = {},
                             std::string tool_id = {},
                             std::string argument_key = {},
                             std::string argument_value = {}) {
    ToolbarActionSpec action;
    action.id = std::move(id);
    action.label = std::move(label);
    action.tool_tip = std::move(tool_tip);
    action.page = std::move(page);
    action.group = std::move(group);
    action.icon_id = std::move(icon_id);
    action.tool_id = std::move(tool_id);
    action.argument_key = std::move(argument_key);
    action.argument_value = std::move(argument_value);
    action.command = command;
    return action;
}

std::string SanitizeActionToken(std::string_view value) {
    std::string token;
    token.reserve(value.size());
    for (const unsigned char ch : value) {
        token.push_back(std::isalnum(ch) ? static_cast<char>(std::tolower(ch)) : '-');
    }
    return token;
}

std::string JoinAddresses(const std::vector<std::string>& addresses) {
    std::string joined;
    for (std::size_t index = 0; index < addresses.size(); ++index) {
        if (index != 0) {
            joined += ", ";
        }
        joined += addresses[index];
    }
    return joined;
}

std::string FallbackIconIdForAction(const ToolbarActionSpec& action) {
    if (!action.icon_id.empty()) {
        return action.icon_id;
    }
    if (action.id.rfind("mailbox:", 0) == 0) {
        return "mail-folder";
    }
    if (action.id.rfind("nickname:", 0) == 0 || action.id.rfind("persona:", 0) == 0) {
        return "person";
    }
    if (action.id.rfind("stationery:", 0) == 0) {
        return "signature";
    }
    if (action.id.rfind("plugin:", 0) == 0) {
        return "plugin";
    }
    if (action.page == "Mailboxes") {
        return "mail-folder";
    }
    if (action.page == "Recipients" || action.page == "Personalities") {
        return "person";
    }
    if (action.page == "Search") {
        return "search";
    }
    if (action.page == "Stationery") {
        return "signature";
    }
    if (action.page == "Plugins") {
        return "plugin";
    }
    return "file-generic";
}

std::vector<ToolbarActionSpec> BuildMainActions() {
    return {
        MakeAction("new", "New Message", "Compose a new message", "General", "File", 'ncmp', "mail-new"),
        MakeAction("check",
                   "Check Mail",
                   "Check all enabled accounts",
                   "General",
                   "Mail",
                   'ckml',
                   "folder-download"),
        MakeAction("send",
                   "Send Queued",
                   "Send queued outgoing messages",
                   "General",
                   "Mail",
                   'sndq',
                   "mail-send"),
        MakeAction("send-receive",
                   "Send & Receive",
                   "Send queued mail and check for new mail",
                   "General",
                   "Mail",
                   'sdrx',
                   "folder-download"),
        MakeAction("work-offline",
                   "Work Offline",
                   "Toggle work offline mode",
                   "General",
                   "Special",
                   'spof'),
        MakeAction("compact-mailboxes",
                   "Compact Mailboxes",
                   "Compact local mailbox storage",
                   "Mailboxes",
                   "Special",
                   'spcm'),
        MakeAction("stop",
                   "Stop Tasks",
                   "Stop active background tasks",
                   "General",
                   "Mail",
                   'stpt',
                   "stop"),
        MakeAction("refresh-mailbox",
                   "Refresh Mailbox",
                   "Refresh the current mailbox",
                   "Mailboxes",
                   "Mailbox",
                   'mbrf'),
        MakeAction("resync-mailbox",
                   "Resync Mailbox",
                   "Fully resync the current IMAP mailbox",
                   "Mailboxes",
                   "Mailbox",
                   'mbrs'),
        MakeAction("open-message",
                   "Open Message",
                   "Open the selected message in its own window",
                   "General",
                   "Message",
                   'msgo'),
        MakeAction("reply",
                   "Reply",
                   "Reply to the selected message",
                   "Recipients",
                   "Message",
                   'rply',
                   "mail-reply"),
        MakeAction("reply-all",
                   "Reply All",
                   "Reply to all recipients",
                   "Recipients",
                   "Message",
                   'rpal',
                   "mail-reply"),
        MakeAction("forward",
                   "Forward",
                   "Forward the selected message",
                   "Recipients",
                   "Message",
                   'frwd',
                   "mail-forward"),
        MakeAction("redirect", "Redirect", "Redirect the selected message", "Recipients", "Message", 'rdrt'),
        MakeAction("send-again",
                   "Send Again",
                   "Open the selected outgoing message for resending",
                   "Recipients",
                   "Message",
                   'sagn'),
        MakeAction("send-immediately",
                   "Send Immediately",
                   "Send the selected outgoing message now",
                   "General",
                   "Message",
                   'simm'),
        MakeAction("previous-message",
                   "Previous Message",
                   "Select the previous message in the current mailbox",
                   "Mailboxes",
                   "Message",
                   'mprv'),
        MakeAction("next-message",
                   "Next Message",
                   "Select the next message in the current mailbox",
                   "Mailboxes",
                   "Message",
                   'mnxt'),
        MakeAction("change-queueing",
                   "Change Queueing",
                   "Adjust the send timing for the selected Out messages",
                   "Mailboxes",
                   "Message",
                   'chqu'),
        MakeAction("delete", "Delete", "Delete the selected message", "General", "Message", 'msgd'),
        MakeAction("undelete",
                   "Undelete",
                   "Restore the selected deleted message",
                   "Mailboxes",
                   "Message",
                   'msgu'),
        MakeAction("mark-read",
                   "Mark Read",
                   "Mark the selected messages as read",
                   "Mailboxes",
                   "Message",
                   'msgr'),
        MakeAction("mark-unread",
                   "Mark Unread",
                   "Mark the selected messages as unread",
                   "Mailboxes",
                   "Message",
                   'mkun'),
        MakeAction("toggle-read",
                   "Toggle Read Status",
                   "Toggle the read state of the selected messages",
                   "Mailboxes",
                   "Message",
                   'mtgl'),
        MakeAction("status-replied",
                   "Status: Replied",
                   "Mark the selected messages as replied",
                   "Mailboxes",
                   "Message",
                   'msts',
                   {},
                   "legacy_status",
                   "2"),
        MakeAction("status-forwarded",
                   "Status: Forwarded",
                   "Mark the selected messages as forwarded",
                   "Mailboxes",
                   "Message",
                   'msts',
                   {},
                   "legacy_status",
                   "3"),
        MakeAction("status-redirected",
                   "Status: Redirected",
                   "Mark the selected messages as redirected",
                   "Mailboxes",
                   "Message",
                   'msts',
                   {},
                   "legacy_status",
                   "4"),
        MakeAction("status-unsendable",
                   "Status: Unsendable",
                   "Mark the selected messages as unsendable",
                   "Mailboxes",
                   "Message",
                   'msts',
                   {},
                   "legacy_status",
                   "5"),
        MakeAction("status-sendable",
                   "Status: Sendable",
                   "Mark the selected messages as sendable",
                   "Mailboxes",
                   "Message",
                   'msts',
                   {},
                   "legacy_status",
                   "6"),
        MakeAction("status-queued",
                   "Status: Queued",
                   "Mark the selected messages as queued",
                   "Mailboxes",
                   "Message",
                   'msts',
                   {},
                   "legacy_status",
                   "7"),
        MakeAction("status-time-queued",
                   "Status: Time Queued",
                   "Mark the selected messages as time queued",
                   "Mailboxes",
                   "Message",
                   'msts',
                   {},
                   "legacy_status",
                   "8"),
        MakeAction("status-sent",
                   "Status: Sent",
                   "Mark the selected messages as sent",
                   "Mailboxes",
                   "Message",
                   'msts',
                   {},
                   "legacy_status",
                   "9"),
        MakeAction("status-unsent",
                   "Status: Unsent",
                   "Mark the selected messages as unsent",
                   "Mailboxes",
                   "Message",
                   'msts',
                   {},
                   "legacy_status",
                   "10"),
        MakeAction("purge-mailbox",
                   "Purge/Expunge",
                   "Permanently remove deleted IMAP messages from the current mailbox",
                   "Mailboxes",
                   "Message",
                   'mpge'),
        MakeAction("move", "Move", "Move the selected message", "Mailboxes", "Message", 'msgm'),
        MakeAction("copy", "Copy", "Copy the selected message", "Mailboxes", "Message", 'msgc'),
        MakeAction("fetch-full",
                   "Fetch Full",
                   "Fetch the full selected IMAP message",
                   "Mailboxes",
                   "Message",
                   'msgf'),
        MakeAction("fetch-default",
                   "Fetch Default",
                   "Fetch the default body for the selected IMAP message",
                   "Mailboxes",
                   "Message",
                   'msdf'),
        MakeAction("redownload-full",
                   "Redownload Full",
                   "Clear cached data and redownload the full selected IMAP message",
                   "Mailboxes",
                   "Message",
                   'irdf'),
        MakeAction("redownload-default",
                   "Redownload Default",
                   "Clear cached data and redownload the default selected IMAP message",
                   "Mailboxes",
                   "Message",
                   'irdd'),
        MakeAction("clear-cached",
                   "Clear Cached",
                   "Clear cached IMAP message data for the selected message",
                   "Mailboxes",
                   "Message",
                   'icch'),
        MakeAction("find-messages",
                   "Find Messages",
                   "Search within the current mailbox",
                   "Mailboxes",
                   "Search",
                   'fdmg',
                   "search"),
        MakeAction("junk", "Junk", "Mark the selected messages as junk", "Mailboxes", "Message", 'mjnk'),
        MakeAction("not-junk",
                   "Not Junk",
                   "Mark the selected messages as not junk",
                   "Mailboxes",
                   "Message",
                   'mnoj'),
        MakeAction("recheck-junk",
                   "Recheck Junk",
                   "Run junk scoring again on the selected messages",
                   "Mailboxes",
                   "Message",
                   'mrjk'),
        MakeAction("server-leave",
                   "Leave",
                   "Leave the selected POP messages on the server",
                   "Mailboxes",
                   "Server",
                   'pslv'),
        MakeAction("server-fetch",
                   "Fetch",
                   "Fetch the selected POP messages from the server",
                   "Mailboxes",
                   "Server",
                   'psft'),
        MakeAction("server-delete",
                   "Delete",
                   "Delete the selected POP messages from the server",
                   "Mailboxes",
                   "Server",
                   'psdl'),
        MakeAction("server-fetch-delete",
                   "Fetch + Delete",
                   "Fetch and then delete the selected POP messages from the server",
                   "Mailboxes",
                   "Server",
                   'psfd'),
        MakeAction("filter-messages",
                   "Filter Messages",
                   "Run manual filters over the selected messages",
                   "Mailboxes",
                   "Search",
                   'mflt'),
        MakeAction("make-filter",
                   "Make Filter",
                   "Create a filter from the selected messages",
                   "Mailboxes",
                   "Search",
                   'mkfl'),
        MakeAction("make-nickname",
                   "Make Nickname",
                   "Create a nickname from the selected sender",
                   "Recipients",
                   "Message",
                   'mknk'),
        MakeAction("print-preview",
                   "Print Preview",
                   "Preview the selected message for printing",
                   "General",
                   "Message",
                   'prpv',
                   "printer"),
        MakeAction("print",
                   "Print One",
                   "Print the selected message",
                   "General",
                   "Message",
                   'prdr',
                   "printer"),
        MakeAction("mailboxes",
                   "Mailboxes",
                   "Show the Mailboxes Wazoo window",
                   "Mailboxes",
                   "Windows",
                   'otwl',
                   "mail-folder",
                   "mailboxes"),
        MakeAction("tools",
                   "Tools",
                   "Show the Tools Wazoo window",
                   "General",
                   "Windows",
                   'otwl',
                   "settings",
                   "nicknames"),
        MakeAction("tasks",
                   "Tasks",
                   "Show the Tasks Wazoo window",
                   "General",
                   "Windows",
                   'otwl',
                   "task",
                   "task-status"),
        MakeAction("search",
                   "Search",
                   "Show the Search Wazoo pane",
                   "General",
                   "Windows",
                   'otwl',
                   "search",
                    "search"),
        MakeAction("plugins",
                   "Plugins",
                   "Show the Plugins Wazoo pane",
                   "Plugins",
                   "Windows",
                   'otwl',
                   "plugin",
                   "plugins"),
        MakeAction("nicknames",
                   "Nicknames",
                   "Show the Nicknames Wazoo pane",
                   "Recipients",
                   "People",
                   'otwl',
                   "person",
                   "nicknames"),
        MakeAction("filters",
                   "Filters",
                   "Show the Filters Wazoo pane",
                   "Recipients",
                   "People",
                   'otwl',
                   "folder-queries",
                   "filters"),
        MakeAction("personalities",
                   "Personalities",
                   "Show the Personalities Wazoo pane",
                   "Personalities",
                   "People",
                   'otwl',
                   "person",
                   "personalities"),
        MakeAction("stationery",
                   "Stationery",
                   "Show the Stationery Wazoo pane",
                   "Stationery",
                   "People",
                   'otwl',
                   "signature",
                   "stationery"),
    };
}

std::vector<ToolbarActionSpec> BuildComposeActions() {
    return {
        MakeAction("queue", "Queue", "Queue this message", "General", "Message", 'queu', "mail-send"),
        MakeAction("send-immediately",
                   "Send Immediately",
                   "Send this message immediately",
                   "General",
                   "Message",
                   'simm',
                   "mail-send"),
        MakeAction("save-draft",
                   "Save Draft",
                   "Save this message as a draft",
                   "General",
                   "File",
                   'sdrf',
                   "save"),
        MakeAction("save-stationery",
                   "Save as Stationery",
                   "Save this message as stationery",
                   "Stationery",
                   "File",
                   'svst'),
        MakeAction("attach",
                   "Attach",
                   "Add an attachment",
                   "General",
                   "Insert",
                   'atag',
                   "file-generic"),
        MakeAction("insert-picture",
                   "Insert Picture",
                   "Insert an inline picture on the HTML surface",
                   "General",
                   "Insert",
                   'insp'),
        MakeAction("insert-system-config",
                   "System Config",
                   "Insert system configuration into the message",
                   "General",
                   "Insert",
                   'insc'),
        MakeAction("check-document",
                   "Check Spelling",
                   "Run spell check on the message",
                   "General",
                   "Spelling",
                   'spck',
                   "search"),
        MakeAction("find",
                   "Find",
                   "Find text in the active compose target",
                   "General",
                   "Edit",
                   'find',
                   "search"),
        MakeAction("find-again",
                   "Find Again",
                   "Find the next matching text",
                   "General",
                   "Edit",
                   'fdag'),
        MakeAction("paste-special",
                   "Paste Special",
                   "Paste from the clipboard using the active editor surface",
                   "General",
                   "Edit",
                   'pspc'),
        MakeAction("paste-quotation",
                   "Paste as Quotation",
                   "Paste clipboard text as quoted content",
                   "General",
                   "Edit",
                   'psqt'),
        MakeAction("toggle-headers",
                   "Toggle Headers",
                   "Show or hide extended headers",
                   "Personalities",
                   "View",
                   'tghd'),
    };
}

std::vector<std::string> BuildMainDefaults() {
    return {"new", "-", "check", "send", "send-receive", "-", "reply", "reply-all", "forward", "-",
            "find-messages", "search", "-", "stop"};
}

std::vector<std::string> BuildComposeDefaults() {
    return {"send-immediately", "queue", "save-draft", "-", "attach", "-", "check-document", "-",
            "find"};
}

std::vector<std::string> UniqueGroups(const std::vector<ToolbarActionSpec>& actions) {
    std::vector<std::string> groups;
    std::set<std::string> seen;
    for (const auto& action : actions) {
        if (seen.insert(action.group).second) {
            groups.push_back(action.group);
        }
    }
    return groups;
}

std::vector<std::string> UniquePages(const std::vector<ToolbarActionSpec>& actions) {
    std::vector<std::string> pages;
    std::set<std::string> seen;
    for (const auto& action : actions) {
        if (seen.insert(action.page).second) {
            pages.push_back(action.page);
        }
    }
    return pages;
}

std::vector<std::string> UniqueGroupsForPage(const std::vector<ToolbarActionSpec>& actions,
                                             std::string_view page) {
    std::vector<std::string> groups;
    std::set<std::string> seen;
    for (const auto& action : actions) {
        if (action.page != page) {
            continue;
        }
        if (seen.insert(action.group).second) {
            groups.push_back(action.group);
        }
    }
    return groups;
}

void ClearChildren(BView& parent) {
    while (BView* child = parent.ChildAt(0)) {
        child->RemoveSelf();
        delete child;
    }
}

}  // namespace

std::vector<ToolbarActionSpec> MainToolbarActionSpecs(HaikuShellHost& shell_host) {
    auto actions = BuildMainActions();

    for (const auto& mailbox : shell_host.Workspace().Mailboxes()) {
        actions.push_back(MakeAction("mailbox:" + SanitizeActionToken(mailbox.id),
                                     mailbox.display_name,
                                     "Open mailbox " + mailbox.display_name,
                                     "Mailboxes",
                                     "Discovered",
                                     kToolbarOpenMailboxMessage,
                                     {},
                                     {},
                                     "mailbox_id",
                                     mailbox.id));
    }

    for (const auto& entry : shell_host.Nicknames().Entries()) {
        const std::string label = entry.nickname.empty() ? "(unnamed nickname)" : entry.nickname;
        const std::string addresses = JoinAddresses(entry.addresses);
        actions.push_back(MakeAction("nickname:" + SanitizeActionToken(entry.nickname),
                                     label,
                                     addresses.empty() ? "Compose a message to " + label : addresses,
                                     "Recipients",
                                     "Nicknames",
                                     kToolbarComposeNicknameMessage,
                                     {},
                                     {},
                                     "nickname",
                                     entry.nickname));
    }

    for (const auto& stationery : shell_host.Stationery().Templates()) {
        actions.push_back(MakeAction("stationery:" + SanitizeActionToken(stationery.name),
                                     stationery.name,
                                     "Compose with stationery " + stationery.name,
                                     "Stationery",
                                     "Templates",
                                     kToolbarComposeStationeryMessage,
                                     {},
                                     {},
                                     "name",
                                     stationery.name));
    }

    for (const auto& account : shell_host.Accounts().Accounts()) {
        const std::string label = account.display_name.empty() ? account.id : account.display_name;
        const std::string tool_tip =
            account.email_address.empty() ? "Compose as " + label : account.email_address;
        actions.push_back(MakeAction("persona:" + SanitizeActionToken(account.id),
                                     label,
                                     tool_tip,
                                     "Personalities",
                                     "Accounts",
                                     kToolbarComposePersonaMessage,
                                     {},
                                     {},
                                     "persona_id",
                                     account.id));
    }

    for (const auto& plugin : shell_host.Plugins().Plugins()) {
        const std::string label = plugin.display_name.empty() ? plugin.identifier : plugin.display_name;
        const std::string identity = plugin.identifier.empty() ? plugin.path.filename().string() : plugin.identifier;
        actions.push_back(MakeAction("plugin:" + SanitizeActionToken(identity),
                                     label.empty() ? plugin.path.filename().string() : label,
                                     plugin.path.string(),
                                     "Plugins",
                                     "Installed",
                                     kToolbarRevealPluginMessage,
                                     {},
                                     {},
                                     "path",
                                     plugin.path.string()));
    }
    return actions;
}

std::vector<ToolbarActionSpec> ComposeToolbarActionSpecs(HaikuShellHost& shell_host) {
    auto actions = BuildComposeActions();
    for (const auto& stationery : shell_host.Stationery().Templates()) {
        actions.push_back(MakeAction("stationery:" + SanitizeActionToken(stationery.name),
                                     stationery.name,
                                     "Apply stationery " + stationery.name,
                                     "Stationery",
                                     "Templates",
                                     'stny',
                                     {},
                                     {},
                                     "name",
                                     stationery.name));
    }
    for (const auto& account : shell_host.Accounts().Accounts()) {
        const std::string label = account.display_name.empty() ? account.id : account.display_name;
        actions.push_back(MakeAction("persona:" + SanitizeActionToken(account.id),
                                     label,
                                     account.email_address.empty() ? label : account.email_address,
                                     "Personalities",
                                     "Accounts",
                                     'pers',
                                     {},
                                     {},
                                     "persona_id",
                                     account.id));
    }
    return actions;
}

const std::vector<std::string>& MainToolbarDefaultEntries() {
    static const auto defaults = BuildMainDefaults();
    return defaults;
}

const std::vector<std::string>& ComposeToolbarDefaultEntries() {
    static const auto defaults = BuildComposeDefaults();
    return defaults;
}

std::vector<std::string> ToolbarAllowedEntries(const std::vector<ToolbarActionSpec>& actions) {
    std::vector<std::string> allowed;
    allowed.reserve(actions.size());
    for (const auto& action : actions) {
        allowed.push_back(action.id);
    }
    return allowed;
}

const ToolbarActionSpec* FindToolbarAction(const std::vector<ToolbarActionSpec>& actions,
                                          std::string_view action_id) {
    const auto it = std::find_if(actions.begin(), actions.end(), [&](const auto& action) {
        return action.id == action_id;
    });
    return it == actions.end() ? nullptr : &*it;
}

void PopulateToolbar(BToolBar& toolbar,
                     BHandler* target,
                     const std::vector<ToolbarActionSpec>& actions,
                     const hermes::ToolbarConfiguration& configuration,
                     bool show_tool_tips,
                     bool large_buttons) {
    ClearChildren(toolbar);

    for (const auto& entry : configuration.visible_entries) {
        if (hermes::ToolbarEntryIsSeparator(entry)) {
            toolbar.AddSeparator();
            continue;
        }

        const ToolbarActionSpec* action = FindToolbarAction(actions, entry);
        if (action == nullptr) {
            continue;
        }

        auto* message = new BMessage(action->command);
        if (!action->tool_id.empty()) {
            message->AddString("tool_id", action->tool_id.c_str());
        }
        if (!action->argument_key.empty()) {
            message->AddString(action->argument_key.c_str(), action->argument_value.c_str());
        }
        const BBitmap* icon = FindToolbarIcon(FallbackIconIdForAction(*action), large_buttons);
        toolbar.AddAction(message,
                          target,
                          icon,
                          show_tool_tips && !action->tool_tip.empty() ? action->tool_tip.c_str()
                                                                      : nullptr,
                          action->label.c_str());
    }
    toolbar.AddGlue();

    for (int32 index = 0; BView* child = toolbar.ChildAt(index); ++index) {
        const float min_height = large_buttons ? 34.0f : 24.0f;
        child->SetExplicitMinSize(BSize(B_SIZE_UNSET, min_height));
    }
}

HaikuToolbarCustomizationWindow::HaikuToolbarCustomizationWindow(
    std::string title,
    std::vector<ToolbarActionSpec> actions,
    std::vector<std::string> default_entries,
    hermes::ToolbarConfiguration configuration,
    bool show_tool_tips,
    bool large_buttons,
    ApplyCallback apply_callback)
    : BWindow(BRect(220, 220, 840, 620),
              title.c_str(),
              B_TITLED_WINDOW,
              B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
      actions_(std::move(actions)),
      default_entries_(std::move(default_entries)),
      configuration_(std::move(configuration)),
      show_tool_tips_(show_tool_tips),
      large_buttons_(large_buttons),
      apply_callback_(std::move(apply_callback)) {
    page_field_ = new BMenuField("toolbar-page", "Page", new BPopUpMenu("toolbar-pages"));
    group_field_ = new BMenuField("toolbar-group", "Group", new BPopUpMenu("toolbar-groups"));
    tool_tips_box_ = new BCheckBox("toolbar-tooltips", "Show Tooltips", nullptr);
    large_buttons_box_ = new BCheckBox("toolbar-large-buttons", "Large Buttons", nullptr);
    available_list_ = new BListView("toolbar-available");
    current_list_ = new BListView("toolbar-current");

    auto* available_scroll =
        new BScrollView("toolbar-available-scroll", available_list_, 0, false, true);
    auto* current_scroll =
        new BScrollView("toolbar-current-scroll", current_list_, 0, false, true);

    auto* add_button = new BButton("toolbar-add", "Add →", new BMessage(kAddMessage));
    auto* remove_button = new BButton("toolbar-remove", "← Remove", new BMessage(kRemoveMessage));
    auto* up_button = new BButton("toolbar-up", "Move Up", new BMessage(kMoveUpMessage));
    auto* down_button = new BButton("toolbar-down", "Move Down", new BMessage(kMoveDownMessage));
    auto* separator_button =
        new BButton("toolbar-separator", "Add Separator", new BMessage(kAddSeparatorMessage));
    auto* reset_button = new BButton("toolbar-reset", "Reset", new BMessage(kResetMessage));
    auto* apply_button = new BButton("toolbar-apply", "Apply", new BMessage(kApplyMessage));
    auto* close_button = new BButton("toolbar-close", "Close", new BMessage(B_QUIT_REQUESTED));

    BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
        .SetInsets(B_USE_DEFAULT_SPACING)
        .AddGroup(B_HORIZONTAL, 8)
            .Add(page_field_)
            .Add(group_field_)
        .End()
        .AddGroup(B_HORIZONTAL, 12)
            .Add(tool_tips_box_)
            .Add(large_buttons_box_)
            .AddGlue()
        .End()
        .AddGroup(B_HORIZONTAL, 10)
            .AddGroup(B_VERTICAL, 6)
                .Add(new BStringView(nullptr, "Available Commands"))
                .Add(available_scroll)
            .End()
            .AddGroup(B_VERTICAL, 6)
                .AddGlue()
                .Add(add_button)
                .Add(remove_button)
                .Add(separator_button)
                .Add(up_button)
                .Add(down_button)
                .AddGlue()
            .End()
            .AddGroup(B_VERTICAL, 6)
                .Add(new BStringView(nullptr, "Current Toolbar"))
                .Add(current_scroll)
            .End()
        .End()
        .AddGroup(B_HORIZONTAL, 8)
            .Add(reset_button)
            .AddGlue()
            .Add(apply_button)
            .Add(close_button)
        .End();

    PopulateGroups();
    SyncPreferenceCheckboxes();
    PopulateCurrentEntries();
    PopulateAvailableEntries();
}

void HaikuToolbarCustomizationWindow::MessageReceived(BMessage* message) {
    const auto allowed = ToolbarAllowedEntries(actions_);
    switch (message->what) {
        case kPageChangedMessage:
            PopulateGroups();
            PopulateAvailableEntries();
            return;
        case kGroupChangedMessage:
            PopulateAvailableEntries();
            return;
        case kAddMessage: {
            const std::string action_id = SelectedAvailableActionId();
            if (!action_id.empty()) {
                const int32 selection = SelectedCurrentIndex();
                const std::size_t position = selection < 0
                                                 ? configuration_.visible_entries.size()
                                                 : static_cast<std::size_t>(selection + 1);
                if (hermes::InsertToolbarEntry(configuration_, allowed, action_id, position)) {
                    PopulateCurrentEntries();
                    PopulateAvailableEntries();
                }
            }
            return;
        }
        case kRemoveMessage: {
            const int32 selection = SelectedCurrentIndex();
            if (selection >= 0 &&
                hermes::RemoveToolbarEntry(configuration_, static_cast<std::size_t>(selection))) {
                PopulateCurrentEntries();
                PopulateAvailableEntries();
            }
            return;
        }
        case kMoveUpMessage: {
            const int32 selection = SelectedCurrentIndex();
            if (selection > 0 &&
                hermes::MoveToolbarEntry(configuration_,
                                         static_cast<std::size_t>(selection),
                                         static_cast<std::size_t>(selection - 1))) {
                PopulateCurrentEntries();
                current_list_->Select(selection - 1);
            }
            return;
        }
        case kMoveDownMessage: {
            const int32 selection = SelectedCurrentIndex();
            if (selection >= 0 &&
                selection + 1 < current_list_->CountItems() &&
                hermes::MoveToolbarEntry(configuration_,
                                         static_cast<std::size_t>(selection),
                                         static_cast<std::size_t>(selection + 1))) {
                PopulateCurrentEntries();
                current_list_->Select(selection + 1);
            }
            return;
        }
        case kAddSeparatorMessage: {
            const int32 selection = SelectedCurrentIndex();
            const std::size_t position = selection < 0
                                             ? configuration_.visible_entries.size()
                                             : static_cast<std::size_t>(selection + 1);
            if (hermes::InsertToolbarEntry(configuration_, allowed, "-", position)) {
                PopulateCurrentEntries();
            }
            return;
        }
        case kResetMessage:
            configuration_ = hermes::ResetToolbarConfiguration(allowed, default_entries_);
            PopulateCurrentEntries();
            PopulateAvailableEntries();
            return;
        case kApplyMessage:
            ApplyChanges();
            return;
        default:
            BWindow::MessageReceived(message);
            return;
    }
}

bool HaikuToolbarCustomizationWindow::QuitRequested() {
    ApplyChanges();
    Hide();
    return false;
}

void HaikuToolbarCustomizationWindow::PopulateGroups() {
    auto* page_menu = page_field_->Menu();
    if (page_menu != nullptr) {
        page_menu->RemoveItems(0, page_menu->CountItems(), true);
        const auto pages = UniquePages(actions_);
        if (!pages.empty()) {
            if (active_page_.empty() ||
                std::find(pages.begin(), pages.end(), active_page_) == pages.end()) {
                active_page_ = pages.front();
            }
        } else {
            active_page_.clear();
        }
        for (const auto& page : pages) {
            auto* item = new BMenuItem(page.c_str(), new BMessage(kPageChangedMessage));
            if (page == active_page_) {
                item->SetMarked(true);
            }
            page_menu->AddItem(item);
        }
    }

    auto* menu = group_field_->Menu();
    menu->RemoveItems(0, menu->CountItems(), true);

    const auto groups = UniqueGroupsForPage(actions_, active_page_);
    if (!groups.empty()) {
        if (active_group_.empty() ||
            std::find(groups.begin(), groups.end(), active_group_) == groups.end()) {
            active_group_ = groups.front();
        }
    } else {
        active_group_.clear();
    }
    for (const auto& group : groups) {
        auto* item = new BMenuItem(group.c_str(), new BMessage(kGroupChangedMessage));
        if (group == active_group_) {
            item->SetMarked(true);
        }
        menu->AddItem(item);
    }
}

void HaikuToolbarCustomizationWindow::PopulateCurrentEntries() {
    current_list_->MakeEmpty();
    for (const auto& entry : configuration_.visible_entries) {
        if (hermes::ToolbarEntryIsSeparator(entry)) {
            current_list_->AddItem(new BStringItem("— Separator —"));
            continue;
        }
        const ToolbarActionSpec* action = FindToolbarAction(actions_, entry);
        current_list_->AddItem(new BStringItem(action != nullptr ? action->label.c_str() : entry.c_str()));
    }
}

void HaikuToolbarCustomizationWindow::PopulateAvailableEntries() {
    available_list_->MakeEmpty();
    available_action_ids_.clear();

    const auto hidden = hermes::HiddenToolbarEntries(configuration_, ToolbarAllowedEntries(actions_));
    for (const auto& action : actions_) {
        if (!active_page_.empty() && action.page != active_page_) {
            continue;
        }
        if (!active_group_.empty() && action.group != active_group_) {
            continue;
        }
        if (std::find(hidden.begin(), hidden.end(), action.id) == hidden.end()) {
            continue;
        }
        available_list_->AddItem(new BStringItem(action.label.c_str()));
        available_action_ids_.push_back(action.id);
    }
}

void HaikuToolbarCustomizationWindow::ApplyChanges() {
    if (tool_tips_box_ != nullptr) {
        show_tool_tips_ = tool_tips_box_->Value() == B_CONTROL_ON;
    }
    if (large_buttons_box_ != nullptr) {
        large_buttons_ = large_buttons_box_->Value() == B_CONTROL_ON;
    }
    if (apply_callback_) {
        apply_callback_(configuration_, show_tool_tips_, large_buttons_);
    }
}

void HaikuToolbarCustomizationWindow::SyncPreferenceCheckboxes() {
    if (tool_tips_box_ != nullptr) {
        tool_tips_box_->SetValue(show_tool_tips_ ? B_CONTROL_ON : B_CONTROL_OFF);
    }
    if (large_buttons_box_ != nullptr) {
        large_buttons_box_->SetValue(large_buttons_ ? B_CONTROL_ON : B_CONTROL_OFF);
    }
}

std::string HaikuToolbarCustomizationWindow::SelectedAvailableActionId() const {
    const int32 selection = available_list_->CurrentSelection();
    if (selection < 0 || static_cast<std::size_t>(selection) >= available_action_ids_.size()) {
        return {};
    }
    return available_action_ids_[static_cast<std::size_t>(selection)];
}

int32_t HaikuToolbarCustomizationWindow::SelectedCurrentIndex() const {
    return current_list_->CurrentSelection();
}

}  // namespace hemera::haiku
