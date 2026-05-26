#include "hermes/MailboxStore.h"

#include <algorithm>
#include <fstream>

#include "hermes/IniSettingsStore.h"

namespace hermes {

FilesystemMailboxStore::FilesystemMailboxStore(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

namespace {

std::filesystem::path MetadataPath(const std::filesystem::path& mailbox_directory) {
    return mailbox_directory / "mailbox.ini";
}

std::size_t CountMessages(const std::filesystem::path& mailbox_directory) {
    if (!std::filesystem::exists(mailbox_directory)) {
        return 0;
    }

    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(mailbox_directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".eml") {
            ++count;
        }
    }
    return count;
}

}  // namespace

bool FilesystemMailboxStore::EnsureMailbox(const MailboxRecord& mailbox, std::string* error_message) {
    if (mailbox.id.empty()) {
        if (error_message) {
            *error_message = "Mailbox id must not be empty.";
        }
        return false;
    }

    const std::filesystem::path mailbox_directory = MailboxesDirectory() / mailbox.id;
    std::error_code create_error;
    std::filesystem::create_directories(mailbox_directory, create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create mailbox directory: " + create_error.message();
        }
        return false;
    }

    IniSettingsStore metadata;
    metadata.SetString("Mailbox", "Id", mailbox.id);
    metadata.SetString("Mailbox", "DisplayName",
                       mailbox.display_name.empty() ? mailbox.id : mailbox.display_name);
    metadata.SetString("Mailbox", "SystemMailbox", mailbox.system_mailbox ? "1" : "0");
    return metadata.SaveToFile(MetadataPath(mailbox_directory), error_message);
}

std::vector<MailboxRecord> FilesystemMailboxStore::ListMailboxes() const {
    std::vector<MailboxRecord> mailboxes;
    const std::filesystem::path directory = MailboxesDirectory();
    if (!std::filesystem::exists(directory)) {
        return mailboxes;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_directory()) {
            continue;
        }

        const std::filesystem::path mailbox_directory = entry.path();
        const std::filesystem::path metadata_path = MetadataPath(mailbox_directory);
        IniSettingsStore metadata;
        if (std::filesystem::exists(metadata_path)) {
            std::string ignored;
            metadata.LoadFromFile(metadata_path, &ignored);
        }

        MailboxRecord record;
        record.id = metadata.GetString("Mailbox", "Id").value_or(mailbox_directory.filename().string());
        record.display_name =
            metadata.GetString("Mailbox", "DisplayName").value_or(record.id);
        record.path = mailbox_directory;
        record.system_mailbox = metadata.GetBool("Mailbox", "SystemMailbox", false);
        record.message_count = CountMessages(mailbox_directory);
        mailboxes.push_back(std::move(record));
    }

    std::sort(mailboxes.begin(),
              mailboxes.end(),
              [](const MailboxRecord& left, const MailboxRecord& right) {
                  return left.display_name < right.display_name;
              });
    return mailboxes;
}

std::optional<MailboxRecord> FilesystemMailboxStore::GetMailbox(std::string_view mailbox_id) const {
    for (const auto& mailbox : ListMailboxes()) {
        if (mailbox.id == mailbox_id) {
            return mailbox;
        }
    }
    return std::nullopt;
}

std::filesystem::path FilesystemMailboxStore::RootDirectory() const {
    return root_directory_;
}

std::filesystem::path FilesystemMailboxStore::MailboxesDirectory() const {
    return root_directory_ / "mailboxes";
}

}  // namespace hermes
