#include "HaikuIconCatalog.h"

#include <Application.h>
#include <Bitmap.h>
#include <Entry.h>
#include <IconUtils.h>
#include <Path.h>

#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace hemera::haiku {

namespace {

std::filesystem::path SourceRoot() {
#ifdef HERMES_SOURCE_ROOT
    return std::filesystem::path(HERMES_SOURCE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

std::filesystem::path ExecutableDirectory() {
    if (be_app == nullptr) {
        return {};
    }
    app_info info {};
    if (be_app->GetAppInfo(&info) != B_OK) {
        return {};
    }
    BEntry entry(&info.ref, true);
    BPath path;
    if (entry.GetPath(&path) != B_OK) {
        return {};
    }
    if (path.GetParent(&path) != B_OK) {
        return {};
    }
    return std::filesystem::path(path.Path());
}

std::vector<std::filesystem::path> CandidateToolbarRoots() {
    std::vector<std::filesystem::path> roots;
    const auto executable_directory = ExecutableDirectory();
    if (!executable_directory.empty()) {
        roots.push_back(executable_directory / "icons" / "toolbar");
    }
    roots.push_back(SourceRoot() / "src" / "hermes_port" / "haiku" / "assets" / "icons" / "toolbar");
    return roots;
}

std::vector<std::uint8_t> ReadBinaryFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        return {};
    }
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)),
                                     std::istreambuf_iterator<char>());
}

std::string CacheKey(std::string_view icon_id, bool large_buttons) {
    return std::string(icon_id) + (large_buttons ? "@large" : "@small");
}

}  // namespace

std::filesystem::path ToolbarIconAssetDirectory() {
    for (const auto& root : CandidateToolbarRoots()) {
        if (std::filesystem::exists(root)) {
            return root;
        }
    }
    return SourceRoot() / "src" / "hermes_port" / "haiku" / "assets" / "icons" / "toolbar";
}

std::filesystem::path AppIconAssetPath() {
    return SourceRoot() / "src" / "hermes_port" / "haiku" / "assets" / "icons" / "app" /
           "hemera-mail.hvif";
}

const BBitmap* FindToolbarIcon(std::string_view icon_id, bool large_buttons) {
    static std::map<std::string, std::shared_ptr<BBitmap>> cache;

    if (icon_id.empty()) {
        return nullptr;
    }

    const auto key = CacheKey(icon_id, large_buttons);
    if (const auto it = cache.find(key); it != cache.end()) {
        return it->second.get();
    }

    const int32 icon_size = large_buttons ? 32 : 24;
    for (const auto& root : CandidateToolbarRoots()) {
        const auto path = root / (std::string(icon_id) + ".hvif");
        if (!std::filesystem::exists(path)) {
            continue;
        }

        const auto bytes = ReadBinaryFile(path);
        if (bytes.empty()) {
            continue;
        }

        auto bitmap = std::make_shared<BBitmap>(BRect(0, 0, icon_size - 1, icon_size - 1), 0, B_RGBA32);
        if (bitmap->InitCheck() != B_OK) {
            continue;
        }
        if (BIconUtils::GetVectorIcon(bytes.data(), bytes.size(), bitmap.get()) != B_OK) {
            continue;
        }
        cache.emplace(key, bitmap);
        return bitmap.get();
    }

    return nullptr;
}

}  // namespace hemera::haiku
