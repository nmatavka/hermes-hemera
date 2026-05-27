#include "HaikuToolWindow.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

#include <Button.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Message.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include "HaikuShellHost.h"

namespace hermes::haiku_port {

namespace {

constexpr uint32_t kSelectionMessage = 'tsel';
constexpr uint32_t kSaveMessage = 'tsav';
constexpr uint32_t kDeleteMessage = 'tdel';
constexpr uint32_t kActionMessage = 'tact';
constexpr uint32_t kNewMessage = 'tnew';

std::string JoinLines(const std::vector<std::string>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            stream << '\n';
        }
        stream << values[index];
    }
    return stream.str();
}

std::vector<std::string> SplitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::string current;
    for (char ch : text) {
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            lines.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty() || lines.empty()) {
        lines.push_back(current);
    }
    return lines;
}

std::string ValueAfterColon(std::string_view line) {
    const std::size_t split = line.find(':');
    if (split == std::string::npos) {
        return {};
    }
    std::string value(line.substr(split + 1));
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    return value;
}

}  // namespace

HaikuToolWindow::HaikuToolWindow(HaikuShellHost& shell_host, std::string tool_id, std::string title)
    : BWindow(BRect(180, 180, 980, 760),
              title.c_str(),
              B_TITLED_WINDOW,
              B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
      shell_host_(shell_host),
      tool_id_(std::move(tool_id)) {
    summary_view_ = new BStringView("tool-summary", title.c_str());
    name_control_ = new BTextControl("tool-name", "Name", "", nullptr);
    aux_control_ = new BTextControl("tool-aux", "Search", "", nullptr);
    item_list_ = new BListView("tool-items");
    item_list_->SetSelectionMessage(new BMessage(kSelectionMessage));
    detail_view_ = new BTextView("tool-detail");
    detail_view_->SetWordWrap(true);
    detail_view_->SetInsets(8, 8, 8, 8);
    save_button_ = new BButton("tool-save", "Save", new BMessage(kSaveMessage));
    delete_button_ = new BButton("tool-delete", "Delete", new BMessage(kDeleteMessage));
    action_button_ = new BButton("tool-action", "Refresh", new BMessage(kActionMessage));
    auto* new_button = new BButton("tool-new", "New", new BMessage(kNewMessage));

    auto* list_scroll = new BScrollView("tool-list-scroll", item_list_, 0, false, true);
    auto* detail_scroll = new BScrollView("tool-detail-scroll", detail_view_, 0, false, true);

    auto* content = new BGroupView(B_VERTICAL);
    BLayoutBuilder::Group<>(content, B_VERTICAL, 8)
        .SetInsets(10, 10, 10, 10)
        .Add(summary_view_)
        .AddGroup(B_HORIZONTAL, 8)
            .Add(name_control_)
            .Add(aux_control_)
        .End()
        .AddGroup(B_HORIZONTAL, 8)
            .Add(list_scroll, 0.38f)
            .Add(detail_scroll, 0.62f)
        .End()
        .AddGroup(B_HORIZONTAL, 8)
            .Add(new_button)
            .Add(save_button_)
            .Add(delete_button_)
            .AddGlue()
            .Add(action_button_)
        .End();

    SetLayout(new BGroupLayout(B_VERTICAL));
    AddChild(content);
    Refresh();
}

void HaikuToolWindow::MessageReceived(BMessage* message) {
    switch (message->what) {
        case kSelectionMessage:
            UpdateDetailFromSelection();
            break;
        case kSaveMessage:
            SaveCurrent();
            break;
        case kDeleteMessage:
            DeleteCurrent();
            break;
        case kActionMessage:
            if (tool_id_ == "mailboxes") {
                const auto mailbox_id = CurrentItemId();
                if (!mailbox_id.empty()) {
                    shell_host_.OpenMailbox(mailbox_id);
                    shell_host_.ReloadWorkspace();
                }
            } else if (tool_id_ == "directory-services") {
                PopulateDirectoryResults();
            } else {
                Refresh();
            }
            break;
        case kNewMessage:
            NewItem();
            break;
        default:
            BWindow::MessageReceived(message);
            break;
    }
}

bool HaikuToolWindow::QuitRequested() {
    Hide();
    return false;
}

void HaikuToolWindow::Refresh() {
    Populate();
}

const std::string& HaikuToolWindow::ToolId() const {
    return tool_id_;
}

void HaikuToolWindow::Populate() {
    items_.clear();
    item_list_->MakeEmpty();
    detail_view_->SetText("");

    if (tool_id_ == "mailboxes") {
        PopulateMailboxes();
    } else if (tool_id_ == "signatures") {
        PopulateSignatures();
    } else if (tool_id_ == "stationery") {
        PopulateStationery();
    } else if (tool_id_ == "nicknames") {
        PopulateNicknames();
    } else if (tool_id_ == "personalities") {
        PopulatePersonalities();
    } else if (tool_id_ == "filters") {
        PopulateFilters();
    } else if (tool_id_ == "filter-report") {
        PopulateFilterReport();
    } else if (tool_id_ == "link-history") {
        PopulateLinkHistory();
    } else if (tool_id_ == "file-browser") {
        PopulateFileBrowser();
    } else if (tool_id_ == "directory-services") {
        PopulateDirectoryResults();
    }

    for (const auto& item : items_) {
        item_list_->AddItem(new BStringItem(item.label.c_str()));
    }
    if (!items_.empty()) {
        item_list_->Select(0);
        UpdateDetailFromSelection();
    }
}

void HaikuToolWindow::PopulateMailboxes() {
    summary_view_->SetText("Mailboxes");
    name_control_->Hide();
    aux_control_->Hide();
    save_button_->Hide();
    delete_button_->Hide();
    action_button_->SetLabel("Open");

    for (const auto& mailbox : shell_host_.Workspace().Mailboxes()) {
        std::ostringstream detail;
        detail << "Id: " << mailbox.id << '\n'
               << "Account: " << mailbox.account_id << '\n'
               << "Protocol: " << mailbox.protocol << '\n'
               << "Unread: " << mailbox.unread_count;
        items_.push_back({mailbox.id, mailbox.display_name, detail.str()});
    }
}

void HaikuToolWindow::PopulateSignatures() {
    summary_view_->SetText("Signatures");
    name_control_->Show();
    aux_control_->Hide();
    save_button_->Show();
    delete_button_->Show();
    action_button_->SetLabel("Refresh");

    for (const auto& signature : shell_host_.Signatures().Templates()) {
        items_.push_back({signature.name,
                          signature.name,
                          signature.body.html_fragment.empty() ? signature.body.plain_text
                                                               : signature.body.html_fragment});
    }
}

void HaikuToolWindow::PopulateStationery() {
    summary_view_->SetText("Stationery");
    name_control_->Show();
    aux_control_->Hide();
    save_button_->Show();
    delete_button_->Show();
    action_button_->SetLabel("Refresh");

    for (const auto& stationery : shell_host_.Stationery().Templates()) {
        std::ostringstream detail;
        if (!stationery.headers.subject.empty()) {
            detail << "Subject: " << stationery.headers.subject << '\n';
        }
        if (!stationery.persona.empty()) {
            detail << "Persona: " << stationery.persona << '\n';
        }
        if (!stationery.signature_name.empty()) {
            detail << "Signature: " << stationery.signature_name << '\n';
        }
        detail << '\n'
               << (stationery.body.html_fragment.empty() ? stationery.body.plain_text
                                                         : stationery.body.html_fragment);
        items_.push_back({stationery.name, stationery.name, detail.str()});
    }
}

void HaikuToolWindow::PopulateNicknames() {
    summary_view_->SetText("Nicknames");
    name_control_->Show();
    aux_control_->Hide();
    save_button_->Show();
    delete_button_->Show();
    action_button_->SetLabel("Refresh");

    for (const auto& nickname : shell_host_.Nicknames().Entries()) {
        std::ostringstream detail;
        detail << "FullName: " << nickname.full_name << '\n'
               << "Addresses: " << JoinLines(nickname.addresses) << '\n'
               << "RecipientList: " << (nickname.recipient_list ? "1" : "0") << '\n'
               << "BPList: " << (nickname.bp_list ? "1" : "0") << '\n'
               << "Notes: " << nickname.notes << '\n';
        items_.push_back({nickname.nickname, nickname.nickname, detail.str()});
    }
}

void HaikuToolWindow::PopulatePersonalities() {
    summary_view_->SetText("Personalities");
    name_control_->Show();
    aux_control_->Hide();
    save_button_->Show();
    delete_button_->Show();
    action_button_->SetLabel("Refresh");

    for (const auto& account : shell_host_.Accounts().Accounts()) {
        std::ostringstream detail;
        detail << "RealName: " << account.display_name << '\n'
               << "Email: " << account.email_address << '\n'
               << "LoginName: " << account.login_name << '\n'
               << "IncomingServer: " << account.incoming_server << '\n'
               << "OutgoingServer: " << account.outgoing_server << '\n'
               << "Protocol: " << (account.uses_imap ? "imap" : "pop") << '\n';
        items_.push_back({account.id, account.display_name, detail.str()});
    }
}

void HaikuToolWindow::PopulateFilters() {
    summary_view_->SetText("Filters");
    name_control_->Show();
    aux_control_->Hide();
    save_button_->Show();
    delete_button_->Show();
    action_button_->SetLabel("Refresh");

    for (const auto& rule : shell_host_.Filters().Rules()) {
        std::ostringstream detail;
        detail << "Field: "
               << (rule.field == hermes::FilterField::kFrom ? "from"
                                                            : rule.field == hermes::FilterField::kTo
                                                                  ? "to"
                                                                  : rule.field == hermes::FilterField::kBody ? "body"
                                                                                                             : "subject")
               << '\n'
               << "Operation: "
               << (rule.operation == hermes::FilterOperation::kEquals
                       ? "equals"
                       : rule.operation == hermes::FilterOperation::kNotContains ? "not-contains"
                                                                                 : "contains")
               << '\n'
               << "Value: " << rule.value << '\n'
               << "DestinationMailbox: "
               << (rule.destination_mailbox ? *rule.destination_mailbox : "") << '\n'
               << "MarkAsRead: " << (rule.mark_as_read ? "1" : "0") << '\n'
               << "MarkAsJunk: " << (rule.mark_as_junk ? "1" : "0") << '\n'
               << "StopProcessing: " << (rule.stop_processing ? "1" : "0") << '\n';
        items_.push_back({rule.name, rule.name, detail.str()});
    }
}

void HaikuToolWindow::PopulateFilterReport() {
    summary_view_->SetText("Filter Report");
    name_control_->Hide();
    aux_control_->Hide();
    save_button_->Hide();
    delete_button_->Hide();
    action_button_->SetLabel("Refresh");

    for (const auto& entry : shell_host_.FilterReport().Entries()) {
        std::ostringstream detail;
        detail << "Mailbox: " << entry.mailbox_name << '\n'
               << "Sender: " << entry.sender << '\n'
               << "Subject: " << entry.subject << '\n'
               << "Matched Rules: " << JoinLines(entry.matched_rules);
        items_.push_back({entry.id, entry.subject.empty() ? entry.message_id : entry.subject, detail.str()});
    }
}

void HaikuToolWindow::PopulateLinkHistory() {
    summary_view_->SetText("Link History");
    name_control_->Hide();
    aux_control_->Hide();
    save_button_->Hide();
    delete_button_->Hide();
    action_button_->SetLabel("Refresh");

    for (const auto& entry : shell_host_.LinkHistory().Entries()) {
        std::ostringstream detail;
        detail << "Target: " << entry.target << '\n'
               << "Source: " << entry.source_context << '\n'
               << "Launched: " << (entry.launched ? "yes" : "no");
        items_.push_back({entry.id, entry.title.empty() ? entry.target : entry.title, detail.str()});
    }
}

void HaikuToolWindow::PopulateFileBrowser() {
    summary_view_->SetText("File Browser");
    name_control_->Hide();
    aux_control_->Hide();
    save_button_->Hide();
    delete_button_->Hide();
    action_button_->SetLabel("Refresh");

    const std::vector<std::filesystem::path> roots = {
        shell_host_.Mailboxes().RootDirectory(),
        shell_host_.SettingsPath().parent_path() / "Attachments",
        shell_host_.Stationery().RootDirectory(),
        shell_host_.Signatures().RootDirectory(),
    };

    for (const auto& root : roots) {
        if (root.empty() || !std::filesystem::exists(root)) {
            continue;
        }
        items_.push_back({root.string(), root.filename().string(), root.string()});
        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            items_.push_back({entry.path().string(),
                              "  " + entry.path().filename().string(),
                              entry.path().string()});
        }
    }
}

void HaikuToolWindow::PopulateDirectoryResults() {
    summary_view_->SetText("Directory Services");
    name_control_->Hide();
    aux_control_->Show();
    save_button_->Hide();
    delete_button_->Hide();
    action_button_->SetLabel("Search");

    const std::string query = aux_control_->Text();
    if (query.empty()) {
        items_.push_back({"providers", "Available Providers", "Enter a query to search local directory sources."});
        for (const auto& provider : shell_host_.DirectoryServices().Providers()) {
            items_.push_back({provider.id, "  " + provider.display_name, provider.id});
        }
        return;
    }

    for (const auto& entry : shell_host_.DirectoryServices().Search(query)) {
        std::ostringstream detail;
        detail << "Provider: " << entry.provider_id << '\n'
               << "Email: " << entry.email_address << '\n'
               << "Notes: " << entry.notes;
        items_.push_back({entry.email_address, entry.display_name, detail.str()});
    }
}

void HaikuToolWindow::SaveCurrent() {
    std::string error_message;
    const std::string name = name_control_->Text();
    const std::string detail = detail_view_->Text();

    if (tool_id_ == "signatures") {
        hermes::SignatureTemplate entry;
        entry.name = name;
        entry.body.plain_text = detail;
        entry.body.html_fragment = detail.find('<') != std::string::npos ? detail : "";
        shell_host_.Signatures().SaveTemplate(entry, &error_message);
    } else if (tool_id_ == "stationery") {
        hermes::StationeryTemplate entry;
        entry.name = name;
        const auto lines = SplitLines(detail);
        bool in_body = false;
        for (const auto& line : lines) {
            if (!in_body && line.empty()) {
                in_body = true;
                continue;
            }
            if (!in_body) {
                if (line.rfind("Subject:", 0) == 0) {
                    entry.headers.subject = ValueAfterColon(line);
                } else if (line.rfind("Persona:", 0) == 0) {
                    entry.persona = ValueAfterColon(line);
                } else if (line.rfind("Signature:", 0) == 0) {
                    entry.signature_name = ValueAfterColon(line);
                }
                continue;
            }
            if (!entry.body.plain_text.empty()) {
                entry.body.plain_text += '\n';
            }
            entry.body.plain_text += line;
        }
        shell_host_.Stationery().SaveTemplate(entry, &error_message);
    } else if (tool_id_ == "nicknames") {
        hermes::NicknameEntry entry;
        entry.nickname = name;
        const auto lines = SplitLines(detail);
        for (const auto& line : lines) {
            if (line.rfind("FullName:", 0) == 0) {
                entry.full_name = ValueAfterColon(line);
            } else if (line.rfind("Addresses:", 0) == 0) {
                entry.addresses = SplitLines(ValueAfterColon(line));
            } else if (line.rfind("RecipientList:", 0) == 0) {
                entry.recipient_list = ValueAfterColon(line) == "1";
            } else if (line.rfind("BPList:", 0) == 0) {
                entry.bp_list = ValueAfterColon(line) == "1";
            } else if (line.rfind("Notes:", 0) == 0) {
                entry.notes = ValueAfterColon(line);
            }
        }
        shell_host_.Nicknames().AddOrReplace(entry);
        shell_host_.Nicknames().SaveToFile(shell_host_.DataRootPath() / "Nicknames.txt", &error_message);
    } else if (tool_id_ == "personalities") {
        auto account = shell_host_.Accounts().FindById(CurrentItemId()).value_or(hermes::AccountProfile{});
        account.id = name;
        account.display_name = name;
        const auto lines = SplitLines(detail);
        for (const auto& line : lines) {
            if (line.rfind("RealName:", 0) == 0) {
                account.display_name = ValueAfterColon(line);
            } else if (line.rfind("Email:", 0) == 0) {
                account.email_address = ValueAfterColon(line);
            } else if (line.rfind("LoginName:", 0) == 0) {
                account.login_name = ValueAfterColon(line);
            } else if (line.rfind("IncomingServer:", 0) == 0) {
                account.incoming_server = ValueAfterColon(line);
            } else if (line.rfind("OutgoingServer:", 0) == 0) {
                account.outgoing_server = ValueAfterColon(line);
            } else if (line.rfind("Protocol:", 0) == 0) {
                const auto protocol = ValueAfterColon(line);
                account.uses_imap = protocol == "imap";
                account.uses_pop = !account.uses_imap;
            }
        }
        shell_host_.Accounts().AddOrReplace(account);
        shell_host_.Accounts().SaveToSettings(shell_host_.Settings(), &error_message);
        shell_host_.PersistSettings(&error_message);
    } else if (tool_id_ == "filters") {
        hermes::FilterRule rule;
        rule.name = name;
        const auto lines = SplitLines(detail);
        for (const auto& line : lines) {
            if (line.rfind("Field:", 0) == 0) {
                const auto value = ValueAfterColon(line);
                rule.field = value == "from"   ? hermes::FilterField::kFrom
                             : value == "to"   ? hermes::FilterField::kTo
                             : value == "body" ? hermes::FilterField::kBody
                                               : hermes::FilterField::kSubject;
            } else if (line.rfind("Operation:", 0) == 0) {
                const auto value = ValueAfterColon(line);
                rule.operation = value == "equals"       ? hermes::FilterOperation::kEquals
                                 : value == "not-contains" ? hermes::FilterOperation::kNotContains
                                                            : hermes::FilterOperation::kContains;
            } else if (line.rfind("Value:", 0) == 0) {
                rule.value = ValueAfterColon(line);
            } else if (line.rfind("DestinationMailbox:", 0) == 0) {
                const auto value = ValueAfterColon(line);
                if (!value.empty()) {
                    rule.destination_mailbox = value;
                }
            } else if (line.rfind("MarkAsRead:", 0) == 0) {
                rule.mark_as_read = ValueAfterColon(line) == "1";
            } else if (line.rfind("MarkAsJunk:", 0) == 0) {
                rule.mark_as_junk = ValueAfterColon(line) == "1";
            } else if (line.rfind("StopProcessing:", 0) == 0) {
                rule.stop_processing = ValueAfterColon(line) != "0";
            }
        }
        shell_host_.Filters().AddOrReplace(rule);
        shell_host_.Filters().SaveToFile(shell_host_.DataRootPath() / "Filters.ini", &error_message);
    }

    Refresh();
}

void HaikuToolWindow::DeleteCurrent() {
    std::string error_message;
    const auto id = CurrentItemId();
    if (id.empty()) {
        return;
    }
    if (tool_id_ == "signatures") {
        shell_host_.Signatures().DeleteTemplate(id, &error_message);
    } else if (tool_id_ == "stationery") {
        shell_host_.Stationery().DeleteTemplate(id, &error_message);
    } else if (tool_id_ == "nicknames") {
        shell_host_.Nicknames().Remove(id);
        shell_host_.Nicknames().SaveToFile(shell_host_.DataRootPath() / "Nicknames.txt", &error_message);
    } else if (tool_id_ == "personalities") {
        shell_host_.Accounts().Remove(id);
        shell_host_.Accounts().SaveToSettings(shell_host_.Settings(), &error_message);
        shell_host_.PersistSettings(&error_message);
    } else if (tool_id_ == "filters") {
        shell_host_.Filters().Remove(id);
        shell_host_.Filters().SaveToFile(shell_host_.DataRootPath() / "Filters.ini", &error_message);
    }
    Refresh();
}

void HaikuToolWindow::NewItem() {
    name_control_->SetText("");
    detail_view_->SetText("");
}

std::string HaikuToolWindow::CurrentItemId() const {
    const int32 selection = item_list_->CurrentSelection();
    if (selection < 0 || static_cast<std::size_t>(selection) >= items_.size()) {
        return {};
    }
    return items_[static_cast<std::size_t>(selection)].id;
}

void HaikuToolWindow::UpdateDetailFromSelection() {
    const int32 selection = item_list_->CurrentSelection();
    if (selection < 0 || static_cast<std::size_t>(selection) >= items_.size()) {
        name_control_->SetText("");
        detail_view_->SetText("");
        return;
    }
    const auto& item = items_[static_cast<std::size_t>(selection)];
    name_control_->SetText(item.label.c_str());
    detail_view_->SetText(item.detail.c_str());
}

}  // namespace hermes::haiku_port
