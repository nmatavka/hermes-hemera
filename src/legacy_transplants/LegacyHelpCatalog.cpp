#include "hermes/HelpCatalog.h"

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

std::vector<HelpTopic> LegacyHelpCatalog::Topics() const {
    return topics_;
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
