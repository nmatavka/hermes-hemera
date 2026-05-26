#include "TestRegistry.h"

#include "hermes/FilesystemPluginHost.h"

HERMES_TEST(FilesystemPluginHostLoadsPortablePluginDescriptor) {
    hermes::FilesystemPluginHost host;
    std::string error_message;
    HERMES_CHECK(host.LoadPlugin(HERMES_SAMPLE_PLUGIN_PATH, &error_message));
    HERMES_CHECK_EQ(host.Plugins().size(), static_cast<std::size_t>(1));
    HERMES_CHECK_EQ(host.Plugins().front().identifier, std::string("sample.status"));
    HERMES_CHECK_EQ(host.Plugins().front().display_name, std::string("Sample Status Plugin"));
}
