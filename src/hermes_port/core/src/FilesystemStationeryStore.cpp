#include "hermes/StationeryStore.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
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

std::string NormalizeValue(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return Trim(normalized);
}

bool LooksLikeHtml(const std::filesystem::path& path, std::string_view body) {
    const std::string extension = NormalizeValue(path.extension().string());
    if (extension == ".html" || extension == ".htm") {
        return true;
    }

    const std::string lowered = NormalizeValue(body);
    return lowered.find("<html") != std::string::npos || lowered.find("<body") != std::string::npos ||
           lowered.find("<p>") != std::string::npos;
}

std::string ReadWholeFile(const std::filesystem::path& path, std::string* error_message) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        if (error_message) {
            *error_message = "Unable to open stationery file: " + path.string();
        }
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool WriteWholeFile(const std::filesystem::path& path,
                    std::string_view contents,
                    std::string* error_message) {
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        if (error_message) {
            *error_message = "Unable to write stationery file: " + path.string();
        }
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(output);
}

}  // namespace

void FilesystemStationeryStore::SetRootDirectory(std::filesystem::path directory) {
    root_directory_ = std::move(directory);
}

std::filesystem::path FilesystemStationeryStore::RootDirectory() const {
    return root_directory_;
}

bool FilesystemStationeryStore::Discover(const std::filesystem::path& directory, std::string* error_message) {
    root_directory_ = directory;
    templates_.clear();
    if (!std::filesystem::exists(directory)) {
        if (error_message) {
            *error_message = "Stationery directory does not exist: " + directory.string();
        }
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::string extension = Normalize(entry.path().extension().string());
        if (extension != ".sta" && extension != ".txt" && extension != ".html" && extension != ".htm") {
            continue;
        }

        auto parsed = ParseTemplate(entry.path(), error_message);
        if (parsed) {
            templates_.push_back(std::move(*parsed));
        }
    }

    std::sort(templates_.begin(), templates_.end(), [](const StationeryTemplate& left, const StationeryTemplate& right) {
        return Normalize(left.name) < Normalize(right.name);
    });
    return true;
}

std::vector<StationeryTemplate> FilesystemStationeryStore::Templates() const {
    return templates_;
}

std::optional<StationeryTemplate> FilesystemStationeryStore::Find(std::string_view name) const {
    const std::string normalized = Normalize(name);
    for (const auto& candidate : templates_) {
        if (Normalize(candidate.name) == normalized) {
            return candidate;
        }
    }
    return std::nullopt;
}

bool FilesystemStationeryStore::SaveTemplate(const StationeryTemplate& stationery,
                                             std::string* error_message) {
    if (root_directory_.empty()) {
        if (error_message) {
            *error_message = "Stationery root directory is not configured.";
        }
        return false;
    }
    if (stationery.name.empty()) {
        if (error_message) {
            *error_message = "Stationery name must not be empty.";
        }
        return false;
    }

    std::error_code create_error;
    std::filesystem::create_directories(root_directory_, create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create stationery directory: " + create_error.message();
        }
        return false;
    }

    const bool html = !stationery.body.html_fragment.empty();
    const auto path = root_directory_ / (stationery.name + (html ? ".html" : ".sta"));
    const auto existing = Find(stationery.name);
    if (existing && existing->source_path != path) {
        std::error_code remove_error;
        std::filesystem::remove(existing->source_path, remove_error);
    }

    std::ostringstream stream;
    if (!stationery.headers.to.empty()) {
        stream << "To: " << stationery.headers.to << '\n';
    }
    if (!stationery.headers.cc.empty()) {
        stream << "Cc: " << stationery.headers.cc << '\n';
    }
    if (!stationery.headers.bcc.empty()) {
        stream << "Bcc: " << stationery.headers.bcc << '\n';
    }
    if (!stationery.headers.subject.empty()) {
        stream << "Subject: " << stationery.headers.subject << '\n';
    }
    if (!stationery.persona.empty()) {
        stream << "Persona: " << stationery.persona << '\n';
    }
    if (!stationery.signature_name.empty()) {
        stream << "Signature: " << stationery.signature_name << '\n';
    }
    stream << '\n' << (html ? stationery.body.html_fragment : stationery.body.plain_text);

    if (!WriteWholeFile(path, stream.str(), error_message)) {
        return false;
    }
    return Discover(root_directory_, error_message);
}

bool FilesystemStationeryStore::DeleteTemplate(std::string_view name, std::string* error_message) {
    const auto existing = Find(name);
    if (!existing) {
        if (error_message) {
            *error_message = "Unknown stationery: " + std::string(name);
        }
        return false;
    }
    std::error_code remove_error;
    std::filesystem::remove(existing->source_path, remove_error);
    if (remove_error) {
        if (error_message) {
            *error_message = "Unable to delete stationery: " + remove_error.message();
        }
        return false;
    }
    return Discover(root_directory_, error_message);
}

std::optional<StationeryTemplate> FilesystemStationeryStore::ParseTemplate(const std::filesystem::path& path,
                                                                           std::string* error_message) {
    const std::string contents = ReadWholeFile(path, error_message);
    if (contents.empty() && !std::filesystem::is_regular_file(path)) {
        return std::nullopt;
    }

    StationeryTemplate result;
    result.name = path.stem().string();
    result.source_path = path;

    std::istringstream stream(contents);
    std::string line;
    bool in_headers = true;
    std::ostringstream body;

    while (std::getline(stream, line)) {
        if (in_headers) {
            const std::string trimmed = Trim(line);
            if (trimmed.empty()) {
                in_headers = false;
                continue;
            }

            const std::size_t separator = trimmed.find(':');
            if (separator != std::string::npos) {
                const std::string key = Normalize(trimmed.substr(0, separator));
                const std::string value = Trim(trimmed.substr(separator + 1));
                if (key == "to") {
                    result.headers.to = value;
                    continue;
                }
                if (key == "cc") {
                    result.headers.cc = value;
                    continue;
                }
                if (key == "bcc") {
                    result.headers.bcc = value;
                    continue;
                }
                if (key == "subject") {
                    result.headers.subject = value;
                    continue;
                }
                if (key == "x-persona" || key == "persona") {
                    result.persona = value;
                    continue;
                }
                if (key == "x-eudora-signature" || key == "signature") {
                    result.signature_name = value;
                    continue;
                }
            }
        }

        body << line;
        if (!stream.eof()) {
            body << '\n';
        }
    }

    const std::string body_text = body.str();
    if (LooksLikeHtml(path, body_text)) {
        result.body.html_fragment = body_text;
    } else {
        result.body.plain_text = body_text;
    }
    return result;
}

std::string FilesystemStationeryStore::Normalize(std::string_view value) {
    return NormalizeValue(value);
}

}  // namespace hermes
