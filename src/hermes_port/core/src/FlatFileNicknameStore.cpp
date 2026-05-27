#include "hermes/NicknameStore.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace hermes {

namespace {

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::vector<std::string> SplitAddresses(std::string_view value) {
    std::vector<std::string> addresses;
    std::string current;
    for (char ch : value) {
        if (ch == ';') {
            const std::string trimmed = Trim(current);
            if (!trimmed.empty()) {
                addresses.push_back(trimmed);
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }

    const std::string trimmed = Trim(current);
    if (!trimmed.empty()) {
        addresses.push_back(trimmed);
    }
    return addresses;
}

std::string JoinAddresses(const std::vector<std::string>& addresses) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < addresses.size(); ++index) {
        if (index != 0) {
            stream << ';';
        }
        stream << addresses[index];
    }
    return stream.str();
}

}  // namespace

bool FlatFileNicknameStore::LoadFromFile(const std::filesystem::path& path, std::string* error_message) {
    std::ifstream input(path);
    if (!input.is_open()) {
        if (error_message) {
            *error_message = "Unable to open nickname file: " + path.string();
        }
        return false;
    }

    entries_.clear();
    NicknameEntry current;
    bool has_current = false;

    std::string line;
    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            if (has_current && !current.nickname.empty()) {
                entries_.push_back(current);
            }

            current = NicknameEntry{};
            has_current = true;
            std::string section_name = line.substr(1, line.size() - 2);
            constexpr std::string_view prefix = "Nickname ";
            if (section_name.rfind(prefix, 0) == 0) {
                current.nickname = Trim(section_name.substr(prefix.size()));
            } else {
                current.nickname = Trim(section_name);
            }
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos || !has_current) {
            continue;
        }

        const std::string key = Normalize(Trim(line.substr(0, separator)));
        const std::string value = Trim(line.substr(separator + 1));
        if (key == "fullname") {
            current.full_name = value;
        } else if (key == "addresses") {
            current.addresses = SplitAddresses(value);
        } else if (key == "notes") {
            current.notes = value;
        } else if (key == "recipientlist") {
            current.recipient_list = value == "1" || Normalize(value) == "true";
        } else if (key == "bplist") {
            current.bp_list = value == "1" || Normalize(value) == "true";
        }
    }

    if (has_current && !current.nickname.empty()) {
        entries_.push_back(current);
    }
    return true;
}

bool FlatFileNicknameStore::SaveToFile(const std::filesystem::path& path, std::string* error_message) const {
    std::ofstream output(path);
    if (!output.is_open()) {
        if (error_message) {
            *error_message = "Unable to write nickname file: " + path.string();
        }
        return false;
    }

    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const NicknameEntry& entry = entries_[index];
        output << "[Nickname " << entry.nickname << "]\n";
        output << "FullName=" << entry.full_name << '\n';
        output << "Addresses=" << JoinAddresses(entry.addresses) << '\n';
        output << "Notes=" << entry.notes << '\n';
        output << "RecipientList=" << (entry.recipient_list ? "1" : "0") << '\n';
        output << "BPList=" << (entry.bp_list ? "1" : "0") << '\n';
        if (index + 1 != entries_.size()) {
            output << '\n';
        }
    }
    return true;
}

std::vector<NicknameEntry> FlatFileNicknameStore::Entries() const {
    return entries_;
}

std::optional<NicknameEntry> FlatFileNicknameStore::FindNickname(std::string_view nickname) const {
    const std::string normalized = Normalize(nickname);
    for (const auto& entry : entries_) {
        if (Normalize(entry.nickname) == normalized) {
            return entry;
        }
    }
    return std::nullopt;
}

void FlatFileNicknameStore::AddOrReplace(const NicknameEntry& entry) {
    const std::string normalized = Normalize(entry.nickname);
    for (auto& existing : entries_) {
        if (Normalize(existing.nickname) == normalized) {
            existing = entry;
            return;
        }
    }
    entries_.push_back(entry);
}

bool FlatFileNicknameStore::Remove(std::string_view nickname) {
    const std::string normalized = Normalize(nickname);
    const auto it = std::remove_if(entries_.begin(), entries_.end(), [&](const NicknameEntry& entry) {
        return Normalize(entry.nickname) == normalized;
    });
    if (it == entries_.end()) {
        return false;
    }
    entries_.erase(it, entries_.end());
    return true;
}

std::string FlatFileNicknameStore::Normalize(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return Trim(normalized);
}

}  // namespace hermes
