#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace hermes {

enum class RendererMode {
    kPaige,
    kWebKit,
    kPlainText,
};

std::string ToString(RendererMode mode);
std::optional<RendererMode> ParseRendererMode(std::string_view value);

}  // namespace hermes
