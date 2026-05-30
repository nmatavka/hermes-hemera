#include "hermes/HelpCatalog.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>

namespace hermes {

namespace {

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::optional<HelpContentsEntry> ParseContentsLine(std::string line, const std::filesystem::path& path) {
    line = Trim(std::move(line));
    if (line.empty() || line[0] == ';' || line[0] == ':') {
        return std::nullopt;
    }

    std::size_t split = 0;
    while (split < line.size() && std::isdigit(static_cast<unsigned char>(line[split]))) {
        ++split;
    }
    if (split == 0 || split >= line.size() || !std::isspace(static_cast<unsigned char>(line[split]))) {
        return std::nullopt;
    }

    HelpContentsEntry entry;
    try {
        entry.level = std::stoi(line.substr(0, split));
    } catch (...) {
        return std::nullopt;
    }

    std::string remainder = Trim(line.substr(split));
    const std::size_t equals = remainder.find('=');
    if (equals == std::string::npos) {
        entry.label = Trim(std::move(remainder));
    } else {
        entry.label = Trim(remainder.substr(0, equals));
        entry.topic_id = Trim(remainder.substr(equals + 1));
    }
    entry.source_path = path;
    if (entry.label.empty()) {
        return std::nullopt;
    }
    return entry;
}

}  // namespace

bool LegacyHelpCatalog::LoadTopicMap(const std::filesystem::path& path, std::string* error_message) {
    std::ifstream input(path);
    if (!input.is_open()) {
        if (error_message) {
            *error_message = "Unable to open help topic map: " + path.string();
        }
        return false;
    }

    topics_.clear();
    std::string line;
    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty() || line.rfind("//", 0) == 0 || line[0] == ';') {
            continue;
        }

        std::istringstream stream(line);
        std::string first;
        std::string second;
        std::string third;
        stream >> first;
        if (first.empty()) {
            continue;
        }

        if (first == "#define") {
            stream >> second >> third;
            if (second.empty()) {
                continue;
            }
            topics_.push_back(HelpTopic{second, second, path});
            continue;
        }

        stream >> second;
        if (second.empty()) {
            continue;
        }
        topics_.push_back(HelpTopic{first, first, path});
    }

    if (topics_.empty() && error_message) {
        *error_message = "No help topics were found in " + path.string();
    }
    return !topics_.empty();
}

bool LegacyHelpCatalog::LoadContents(const std::filesystem::path& path, std::string* error_message) {
    std::ifstream input(path);
    if (!input.is_open()) {
        if (error_message) {
            *error_message = "Unable to open help contents: " + path.string();
        }
        return false;
    }

    contents_.clear();
    std::string line;
    while (std::getline(input, line)) {
        if (const auto entry = ParseContentsLine(std::move(line), path)) {
            contents_.push_back(*entry);
        }
    }

    if (contents_.empty() && error_message) {
        *error_message = "No help contents entries were found in " + path.string();
    }
    return !contents_.empty();
}

std::vector<HelpTopic> LegacyHelpCatalog::Topics() const {
    return topics_;
}

std::vector<HelpContentsEntry> LegacyHelpCatalog::Contents() const {
    return contents_;
}

std::optional<HelpTopic> LegacyHelpCatalog::FindById(std::string_view id) const {
    const auto it = std::find_if(topics_.begin(), topics_.end(), [&](const HelpTopic& topic) {
        return topic.id == id;
    });
    if (it == topics_.end()) {
        return std::nullopt;
    }
    return *it;
}

}  // namespace hermes
