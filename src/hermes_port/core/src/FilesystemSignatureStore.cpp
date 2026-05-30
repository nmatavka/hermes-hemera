#include "hermes/SignatureStore.h"
#include "hermes/RichTextFormat.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
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
    return LooksLikeHtmlDocument(body);
}

bool LooksLikeRtf(const std::filesystem::path& path, std::string_view body) {
    return NormalizeValue(path.extension().string()) == ".rtf" || LooksLikeRtfDocument(body);
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

bool WriteWholeFile(const std::filesystem::path& path,
                    std::string_view contents,
                    std::string* error_message) {
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        if (error_message) {
            *error_message = "Unable to write signature file: " + path.string();
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
                                     const SignatureTemplate& signature,
                                     const std::optional<SignatureTemplate>& existing) {
    if (existing) {
        return existing->source_path;
    }
    if (!signature.source_path.empty() && signature.source_path.parent_path() == root) {
        return signature.source_path;
    }
    const std::string extension =
        signature.body.styled_source == StyledDocumentSource::kRtf  ? ".rtf"
        : signature.body.styled_source == StyledDocumentSource::kHtml ? ".html"
                                                                     : ".sig";
    return root / (signature.name + extension);
}

bool WriteSidecar(const std::filesystem::path& visible_path,
                  const RichTextDocument& body,
                  std::string* error_message) {
    const auto sidecar = SidecarDirectory(visible_path);
    std::error_code create_error;
    std::filesystem::create_directories(sidecar, create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create signature sidecar: " + create_error.message();
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

void FilesystemSignatureStore::SetRootDirectory(std::filesystem::path directory) {
    root_directory_ = std::move(directory);
}

std::filesystem::path FilesystemSignatureStore::RootDirectory() const {
    return root_directory_;
}

bool FilesystemSignatureStore::Discover(const std::filesystem::path& directory, std::string* error_message) {
    root_directory_ = directory;
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
        if (extension != ".sig" && extension != ".txt" && extension != ".html" && extension != ".htm" &&
            extension != ".rtf") {
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

bool FilesystemSignatureStore::SaveTemplate(const SignatureTemplate& signature, std::string* error_message) {
    if (root_directory_.empty()) {
        if (error_message) {
            *error_message = "Signature root directory is not configured.";
        }
        return false;
    }
    if (signature.name.empty()) {
        if (error_message) {
            *error_message = "Signature name must not be empty.";
        }
        return false;
    }

    std::error_code create_error;
    std::filesystem::create_directories(root_directory_, create_error);
    if (create_error) {
        if (error_message) {
            *error_message = "Unable to create signature directory: " + create_error.message();
        }
        return false;
    }

    SignatureTemplate updated = signature;
    updated.body = PrepareRichTextDocumentForPersistence(signature.body);
    const auto old = Find(updated.name);
    const auto path = VisiblePathFor(root_directory_, updated, old);
    if (old && old->source_path != path) {
        std::error_code remove_error;
        std::filesystem::remove(old->source_path, remove_error);
        RemoveSidecar(old->source_path);
    }

    const std::string extension = Normalize(path.extension().string());
    const std::string visible_body =
        extension == ".html" || extension == ".htm" ? updated.body.html_fragment
        : extension == ".rtf"                        ? updated.body.rtf_fragment
                                                      : updated.body.plain_text;
    if (!WriteWholeFile(path, visible_body, error_message) || !WriteSidecar(path, updated.body, error_message)) {
        return false;
    }
    return Discover(root_directory_, error_message);
}

bool FilesystemSignatureStore::DeleteTemplate(std::string_view name, std::string* error_message) {
    const auto existing = Find(name);
    if (!existing) {
        if (error_message) {
            *error_message = "Unknown signature: " + std::string(name);
        }
        return false;
    }
    std::error_code remove_error;
    std::filesystem::remove(existing->source_path, remove_error);
    if (remove_error) {
        if (error_message) {
            *error_message = "Unable to delete signature: " + remove_error.message();
        }
        return false;
    }
    RemoveSidecar(existing->source_path);
    return Discover(root_directory_, error_message);
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
        result.body.plain_text = StripHtml(contents);
        result.body.styled_source = StyledDocumentSource::kHtml;
    } else if (LooksLikeRtf(path, contents)) {
        result.body.rtf_fragment = contents;
        result.body.plain_text = StripRtf(contents);
        result.body.styled_source = StyledDocumentSource::kRtf;
    } else {
        result.body.plain_text = contents;
    }
    result.body.fidelity = ClassifyStyledDocument(result.body);

    const auto source_path = SidecarFile(path, "source.txt");
    if (const std::string source = ReadWholeFile(source_path, nullptr); !source.empty()) {
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

std::string FilesystemSignatureStore::Normalize(std::string_view value) {
    return NormalizeValue(value);
}

}  // namespace hermes
