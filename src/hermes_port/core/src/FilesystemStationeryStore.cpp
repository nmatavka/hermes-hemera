#include "hermes/StationeryStore.h"
#include "hermes/RichTextFormat.h"

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
    return LooksLikeHtmlDocument(body);
}

bool LooksLikeRtf(const std::filesystem::path& path, std::string_view body) {
    return NormalizeValue(path.extension().string()) == ".rtf" || LooksLikeRtfDocument(body);
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

std::filesystem::path SidecarDirectory(const std::filesystem::path& visible_path) {
    return std::filesystem::path(visible_path.string() + ".hermes");
}

std::filesystem::path SidecarFile(const std::filesystem::path& visible_path, const char* name) {
    return SidecarDirectory(visible_path) / name;
}

void RemoveSidecar(const std::filesystem::path& visible_path) {
    std::error_code ignored;
    std::filesystem::remove_all(SidecarDirectory(visible_path), ignored);
}

std::filesystem::path VisiblePathFor(const std::filesystem::path& root,
                                     const StationeryTemplate& stationery,
                                     const std::optional<StationeryTemplate>& existing) {
    if (existing) {
        return existing->source_path;
    }
    if (!stationery.source_path.empty() && stationery.source_path.parent_path() == root) {
        return stationery.source_path;
    }
    const std::string extension =
        stationery.body.styled_source == StyledDocumentSource::kRtf  ? ".rtf"
        : stationery.body.styled_source == StyledDocumentSource::kHtml ? ".html"
                                                                       : ".sta";
    return root / (stationery.name + extension);
}

bool WriteSidecar(const std::filesystem::path& visible_path,
                  const RichTextDocument& body,
                  std::string* error_message) {
    const auto sidecar = SidecarDirectory(visible_path);
    std::error_code create_error;
    std::filesystem::create_directories(sidecar, create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create stationery sidecar: " + create_error.message();
        }
        return false;
    }
    if (!WriteWholeFile(SidecarFile(visible_path, "source.txt"), ToString(body.styled_source), error_message) ||
        !WriteWholeFile(SidecarFile(visible_path, "fidelity.txt"), ToString(body.fidelity), error_message) ||
        !WriteWholeFile(SidecarFile(visible_path, "body.html"), body.html_fragment, error_message) ||
        !WriteWholeFile(SidecarFile(visible_path, "body.rtf"), body.rtf_fragment, error_message) ||
        !WriteWholeFile(SidecarFile(visible_path, "body.pg"), body.paige_native_bytes, error_message)) {
        return false;
    }
    return true;
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
        if (extension != ".sta" && extension != ".txt" && extension != ".html" && extension != ".htm" &&
            extension != ".rtf") {
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

    StationeryTemplate prepared = stationery;
    prepared.body = PrepareRichTextDocumentForPersistence(stationery.body);
    const auto existing = Find(stationery.name);
    const auto path = VisiblePathFor(root_directory_, prepared, existing);
    if (existing && existing->source_path != path) {
        std::error_code remove_error;
        std::filesystem::remove(existing->source_path, remove_error);
        RemoveSidecar(existing->source_path);
    }

    std::ostringstream stream;
    if (!prepared.headers.to.empty()) {
        stream << "To: " << prepared.headers.to << '\n';
    }
    if (!prepared.headers.cc.empty()) {
        stream << "Cc: " << prepared.headers.cc << '\n';
    }
    if (!prepared.headers.bcc.empty()) {
        stream << "Bcc: " << prepared.headers.bcc << '\n';
    }
    if (!prepared.headers.subject.empty()) {
        stream << "Subject: " << prepared.headers.subject << '\n';
    }
    if (!prepared.persona.empty()) {
        stream << "Persona: " << prepared.persona << '\n';
    }
    if (!prepared.signature_name.empty()) {
        stream << "Signature: " << prepared.signature_name << '\n';
    }
    const std::string extension = Normalize(path.extension().string());
    const std::string visible_body =
        extension == ".html" || extension == ".htm" ? prepared.body.html_fragment
        : extension == ".rtf"                        ? prepared.body.rtf_fragment
                                                      : prepared.body.plain_text;
    stream << '\n' << visible_body;

    if (!WriteWholeFile(path, stream.str(), error_message) || !WriteSidecar(path, prepared.body, error_message)) {
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
    RemoveSidecar(existing->source_path);
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
        result.body.plain_text = StripHtml(body_text);
        result.body.styled_source = StyledDocumentSource::kHtml;
    } else if (LooksLikeRtf(path, body_text)) {
        result.body.rtf_fragment = body_text;
        result.body.plain_text = StripRtf(body_text);
        result.body.styled_source = StyledDocumentSource::kRtf;
    } else {
        result.body.plain_text = body_text;
    }
    result.body.fidelity = ClassifyStyledDocument(result.body);

    if (const std::string source = ReadWholeFile(SidecarFile(path, "source.txt"), nullptr); !source.empty()) {
        result.body.styled_source = ParseStyledDocumentSource(source);
    }
    if (const std::string fidelity = ReadWholeFile(SidecarFile(path, "fidelity.txt"), nullptr); !fidelity.empty()) {
        result.body.fidelity = ParseStyledDocumentFidelity(fidelity);
    }
    if (const std::string html = ReadWholeFile(SidecarFile(path, "body.html"), nullptr); !html.empty()) {
        result.body.html_fragment = html;
    }
    if (const std::string rtf = ReadWholeFile(SidecarFile(path, "body.rtf"), nullptr); !rtf.empty()) {
        result.body.rtf_fragment = rtf;
    }
    if (const std::string native = ReadWholeFile(SidecarFile(path, "body.pg"), nullptr); !native.empty()) {
        result.body.paige_native_bytes = native;
    }
    result.body = NormalizeRichTextDocument(result.body);
    return result;
}

std::string FilesystemStationeryStore::Normalize(std::string_view value) {
    return NormalizeValue(value);
}

}  // namespace hermes
