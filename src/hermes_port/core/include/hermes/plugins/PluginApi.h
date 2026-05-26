#pragma once

#include <cstdint>

namespace hermes::plugins {

constexpr std::uint32_t kPluginAbiVersion = 1;

enum PluginCapability : std::uint32_t {
    kPluginCapabilityMenu = 1u << 0,
    kPluginCapabilityTranslator = 1u << 1,
    kPluginCapabilitySpecial = 1u << 2,
    kPluginCapabilitySecurity = 1u << 3,
    kPluginCapabilityMessageProcessor = kPluginCapabilityTranslator,
};

struct PluginDescriptor {
    std::uint32_t abi_version = kPluginAbiVersion;
    const char* identifier = nullptr;
    const char* display_name = nullptr;
    const char* version = nullptr;
    std::uint32_t capabilities = 0;
};

using PluginDescribeFn = bool (*)(PluginDescriptor*);

}  // namespace hermes::plugins
