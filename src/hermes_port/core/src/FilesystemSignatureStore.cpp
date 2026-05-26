#include "hermes/SignatureStore.h"

#include <algorithm>
#include <cctype>
#include <fstream>

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
           lowered.find("<p>") != std::string::npos || lowered.find("<div") != std::string::npos;
}

std::string ReadWholeFile(const std::filesystem::path& path, std::string* error_message) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        if (error_message) {
            *error_message = "Unable to open signature file: " + path.string();
        }
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string StripTags(std::string_view html) {
    std::string result;
    result.reserve(html.size());

    bool inside_tag = false;
    for (char ch : html) {
        if (ch == '<') {
            inside_tag = true;
            continue;
        }
        if (ch == '>') {
            inside_tag = false;
            continue;
        }
        if (!inside_tag) {
            result.push_back(ch);
        }
    }

    return result;
}

}  // namespace

bool FilesystemSignatureStore::Discover(const std::filesystem::path& directory, std::string* error_message) {
    templates_.clear();
    if (!std::filesystem::exists(directory)) {
        if (error_message) {
            *error_message = "Signature directory does not exist: " + directory.string();
        }
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::string extension = Normalize(entry.path().extension().string());
        if (extension != ".sig" && extension != ".txt" && extension != ".html" && extension != ".htm") {
            continue;
        }

        auto parsed = ParseTemplate(entry.path(), error_message);
        if (parsed) {
            templates_.push_back(std::move(*parsed));
        }
    }

    std::sort(templates_.begin(), templates_.end(), [](const SignatureTemplate& left, const SignatureTemplate& right) {
        return Normalize(left.name) < Normalize(right.name);
    });
    return true;
}

std::vector<SignatureTemplate> FilesystemSignatureStore::Templates() const {
    return templates_;
}

std::optional<SignatureTemplate> FilesystemSignatureStore::Find(std::string_view name) const {
    const std::string normalized = Normalize(name);
    for (const auto& candidate : templates_) {
        if (Normalize(candidate.name) == normalized) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::optional<SignatureTemplate> FilesystemSignatureStore::ParseTemplate(const std::filesystem::path& path,
                                                                         std::string* error_message) {
    const std::string contents = ReadWholeFile(path, error_message);
    if (contents.empty() && !std::filesystem::is_regular_file(path)) {
        return std::nullopt;
    }

    SignatureTemplate result;
    result.name = path.stem().string();
    result.source_path = path;

    if (LooksLikeHtml(path, contents)) {
        result.body.html_fragment = contents;
        result.body.plain_text = StripTags(contents);
    } else {
        result.body.plain_text = contents;
    }

    return result;
}

std::string FilesystemSignatureStore::Normalize(std::string_view value) {
    return NormalizeValue(value);
}

}  // namespace hermes
