#include "hermes/LinkHistoryStore.h"

#include <algorithm>

#include "hermes/IniSettingsStore.h"

namespace hermes {

namespace {

std::string SectionName(std::size_t index) {
    return "Entry " + std::to_string(static_cast<unsigned long long>(index));
}

std::string KindToString(LinkHistoryKind kind) {
    switch (kind) {
        case LinkHistoryKind::kUrl:
            return "url";
        case LinkHistoryKind::kAttachment:
            return "attachment";
        case LinkHistoryKind::kFile:
            return "file";
    }
    return "url";
}

LinkHistoryKind KindFromString(std::string_view value) {
    if (value == "attachment") {
        return LinkHistoryKind::kAttachment;
    }
    if (value == "file") {
        return LinkHistoryKind::kFile;
    }
    return LinkHistoryKind::kUrl;
}

}  // namespace

bool FilesystemLinkHistoryStore::LoadFromFile(const std::filesystem::path& path,
                                              std::string* error_message) {
    IniSettingsStore settings;
    if (!settings.LoadFromFile(path, error_message)) {
        return false;
    }

    entries_.clear();
    for (const auto& section : settings.Sections()) {
        if (section.rfind("Entry ", 0) != 0) {
            continue;
        }
        LinkHistoryEntry entry;
        entry.id = settings.GetString(section, "Id").value_or(section);
        entry.kind = KindFromString(settings.GetString(section, "Kind").value_or("url"));
        entry.title = settings.GetString(section, "Title").value_or("");
        entry.target = settings.GetString(section, "Target").value_or("");
        entry.source_context = settings.GetString(section, "SourceContext").value_or("");
        entry.launched = settings.GetBool(section, "Launched", true);
        entry.timestamp = static_cast<std::int64_t>(settings.GetInt(section, "Timestamp", 0));
        entries_.push_back(std::move(entry));
    }
    return true;
}

bool FilesystemLinkHistoryStore::SaveToFile(const std::filesystem::path& path,
                                            std::string* error_message) const {
    IniSettingsStore settings;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const auto section = SectionName(index);
        const auto& entry = entries_[index];
        settings.SetString(section, "Id", entry.id);
        settings.SetString(section, "Kind", KindToString(entry.kind));
        settings.SetString(section, "Title", entry.title);
        settings.SetString(section, "Target", entry.target);
        settings.SetString(section, "SourceContext", entry.source_context);
        settings.SetString(section, "Launched", entry.launched ? "1" : "0");
        settings.SetString(section, "Timestamp", std::to_string(entry.timestamp));
    }
    return settings.SaveToFile(path, error_message);
}

std::vector<LinkHistoryEntry> FilesystemLinkHistoryStore::Entries() const {
    return entries_;
}

std::optional<LinkHistoryEntry> FilesystemLinkHistoryStore::FindById(std::string_view id) const {
    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const auto& entry) {
        return entry.id == id;
    });
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return *it;
}

void FilesystemLinkHistoryStore::AddEntry(const LinkHistoryEntry& entry) {
    entries_.push_back(entry);
}

bool FilesystemLinkHistoryStore::Remove(std::string_view id) {
    const auto it = std::remove_if(entries_.begin(), entries_.end(), [&](const auto& entry) {
        return entry.id == id;
    });
    if (it == entries_.end()) {
        return false;
    }
    entries_.erase(it, entries_.end());
    return true;
}

void FilesystemLinkHistoryStore::Clear() {
    entries_.clear();
}

}  // namespace hermes
