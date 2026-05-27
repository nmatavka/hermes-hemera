#include "TestRegistry.h"

#include "hermes/ToolbarConfiguration.h"

HERMES_TEST(ToolbarConfigurationParsesCustomEntryOrderAndSeparators) {
    const std::vector<std::string> allowed = {"new", "check", "send", "stop"};
    const std::vector<std::string> defaults = {"new", "-", "check", "send"};

    const hermes::ToolbarConfiguration configuration =
        hermes::ParseToolbarConfiguration("send,-,new,send,unknown,-,stop", allowed, defaults);

    HERMES_CHECK_EQ(configuration.visible_entries.size(), static_cast<std::size_t>(5));
    HERMES_CHECK_EQ(configuration.visible_entries[0], std::string("send"));
    HERMES_CHECK_EQ(configuration.visible_entries[1], std::string("-"));
    HERMES_CHECK_EQ(configuration.visible_entries[2], std::string("new"));
    HERMES_CHECK_EQ(configuration.visible_entries[3], std::string("-"));
    HERMES_CHECK_EQ(configuration.visible_entries[4], std::string("stop"));
    HERMES_CHECK_EQ(hermes::SerializeToolbarConfiguration(configuration), std::string("send,-,new,-,stop"));
}

HERMES_TEST(ToolbarConfigurationFallsBackToDefaultsWhenCustomLayoutIsInvalid) {
    const std::vector<std::string> allowed = {"new", "check", "send", "stop"};
    const std::vector<std::string> defaults = {"new", "-", "check", "send"};

    const hermes::ToolbarConfiguration configuration =
        hermes::ParseToolbarConfiguration("bogus,-,missing", allowed, defaults);

    HERMES_CHECK_EQ(configuration.visible_entries.size(), defaults.size());
    HERMES_CHECK_EQ(configuration.visible_entries[0], std::string("new"));
    HERMES_CHECK_EQ(configuration.visible_entries[1], std::string("-"));
    HERMES_CHECK_EQ(configuration.visible_entries[2], std::string("check"));
    HERMES_CHECK_EQ(configuration.visible_entries[3], std::string("send"));
}
