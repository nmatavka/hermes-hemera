#include "hermes/RendererMode.h"

#include <algorithm>
#include <cctype>

namespace hermes {

namespace {

std::string Normalize(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value) {
        if (ch == '-' || ch == '_' || std::isspace(static_cast<unsigned char>(ch))) {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

}  // namespace

std::string ToString(RendererMode mode) {
    switch (mode) {
        case RendererMode::kPaige:
            return "paige";
        case RendererMode::kWebKit:
            return "webkit";
        case RendererMode::kPlainText:
            return "plain-text";
    }

    return "plain-text";
}

std::optional<RendererMode> ParseRendererMode(std::string_view value) {
    const std::string normalized = Normalize(value);
    if (normalized == "paige") {
        return RendererMode::kPaige;
    }
    if (normalized == "webkit") {
        return RendererMode::kWebKit;
    }
    if (normalized == "plaintext") {
        return RendererMode::kPlainText;
    }

    return std::nullopt;
}

}  // namespace hermes
