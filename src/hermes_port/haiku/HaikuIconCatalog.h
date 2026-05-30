#pragma once

#include <filesystem>
#include <string_view>

class BBitmap;

namespace hemera::haiku {

std::filesystem::path ToolbarIconAssetDirectory();
std::filesystem::path AppIconAssetPath();
const BBitmap* FindToolbarIcon(std::string_view icon_id, bool large_buttons);

}  // namespace hemera::haiku
