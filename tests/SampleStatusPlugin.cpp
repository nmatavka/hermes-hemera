#include "hermes/plugins/PluginApi.h"

extern "C" bool HermesPlugin_GetDescriptor(hermes::plugins::PluginDescriptor* descriptor) {
    if (descriptor == nullptr) {
        return false;
    }

    descriptor->abi_version = hermes::plugins::kPluginAbiVersion;
    descriptor->identifier = "sample.status";
    descriptor->display_name = "Sample Status Plugin";
    descriptor->version = "0.1.0";
    descriptor->capabilities = hermes::plugins::kPluginCapabilityMenu;
    return true;
}
