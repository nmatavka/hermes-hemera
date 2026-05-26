#include "hermes/FilesystemPluginHost.h"

#include <unordered_set>

#include "hermes/plugins/PluginApi.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace hermes {

namespace {

bool HasPluginExtension(const std::filesystem::path& path) {
#if defined(_WIN32)
    return path.extension() == ".dll";
#elif defined(__APPLE__)
    return path.extension() == ".bundle" || path.extension() == ".dylib" || path.extension() == ".so";
#else
    return path.extension() == ".so";
#endif
}

}  // namespace

FilesystemPluginHost::~FilesystemPluginHost() {
    for (auto& library : libraries_) {
#if defined(_WIN32)
        if (library->handle != nullptr) {
            FreeLibrary(static_cast<HMODULE>(library.handle));
        }
#else
        if (library.handle != nullptr) {
            dlclose(library.handle);
        }
#endif
    }
}

bool FilesystemPluginHost::Discover(const std::filesystem::path& directory, std::string* error_message) {
    if (!std::filesystem::exists(directory)) {
        if (error_message) {
            *error_message = "Plugin directory does not exist: " + directory.string();
        }
        return false;
    }

    bool discovered_any = false;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || !HasPluginExtension(entry.path())) {
            continue;
        }

        std::string load_error;
        if (LoadPlugin(entry.path(), &load_error)) {
            discovered_any = true;
        } else if (error_message && error_message->empty()) {
            *error_message = load_error;
        }
    }

    return discovered_any;
}

bool FilesystemPluginHost::LoadPlugin(const std::filesystem::path& path, std::string* error_message) {
    for (const auto& plugin : plugins_) {
        if (plugin.path == path) {
            return true;
        }
    }

    LoadedLibrary library;
    library.path = path;

#if defined(_WIN32)
    library.handle = reinterpret_cast<void*>(LoadLibraryA(path.string().c_str()));
    if (library.handle == nullptr) {
        if (error_message) {
            *error_message = "LoadLibrary failed for " + path.string();
        }
        return false;
    }

    auto describe = reinterpret_cast<plugins::PluginDescribeFn>(
        GetProcAddress(static_cast<HMODULE>(library.handle), "HermesPlugin_GetDescriptor"));
#else
    library.handle = dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
    if (library.handle == nullptr) {
        if (error_message) {
            *error_message = dlerror();
        }
        return false;
    }

    auto describe = reinterpret_cast<plugins::PluginDescribeFn>(
        dlsym(library.handle, "HermesPlugin_GetDescriptor"));
#endif

    if (describe == nullptr) {
        if (error_message) {
            *error_message = "Missing HermesPlugin_GetDescriptor entry point in " + path.string();
        }
        return false;
    }

    plugins::PluginDescriptor descriptor;
    if (!describe(&descriptor)) {
        if (error_message) {
            *error_message = "Plugin rejected descriptor request for " + path.string();
        }
        return false;
    }

    if (descriptor.abi_version != plugins::kPluginAbiVersion) {
        if (error_message) {
            *error_message = "Plugin ABI mismatch for " + path.string();
        }
        return false;
    }

    library.summary = PluginSummary{
        path,
        descriptor.identifier ? descriptor.identifier : "",
        descriptor.display_name ? descriptor.display_name : "",
        descriptor.version ? descriptor.version : "",
        descriptor.capabilities,
    };

    plugins_.push_back(library.summary);
    libraries_.push_back(std::move(library));
    return true;
}

const std::vector<PluginSummary>& FilesystemPluginHost::Plugins() const {
    return plugins_;
}

}  // namespace hermes
