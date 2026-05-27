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

HERMES_TEST(ToolbarConfigurationSupportsHiddenEntriesInsertionAndReset) {
    const std::vector<std::string> allowed = {"new", "check", "send", "stop", "attach"};
    const std::vector<std::string> defaults = {"new", "-", "check", "send"};

    hermes::ToolbarConfiguration configuration =
        hermes::ParseToolbarConfiguration("new,send", allowed, defaults);

    const auto hidden = hermes::HiddenToolbarEntries(configuration, allowed);
    HERMES_CHECK_EQ(hidden.size(), static_cast<std::size_t>(3));
    HERMES_CHECK(hermes::InsertToolbarEntry(configuration, allowed, "-", 1));
    HERMES_CHECK(hermes::InsertToolbarEntry(configuration, allowed, "check", 2));
    HERMES_CHECK(hermes::MoveToolbarEntry(configuration, 2, 1));
    HERMES_CHECK(hermes::ToolbarConfigurationContains(configuration, "check"));
    HERMES_CHECK(hermes::RemoveToolbarEntry(configuration, 2));
    HERMES_CHECK_EQ(hermes::SerializeToolbarConfiguration(configuration), std::string("new,check,send"));

    const auto reset = hermes::ResetToolbarConfiguration(allowed, defaults);
    HERMES_CHECK_EQ(reset.visible_entries.size(), defaults.size());
    HERMES_CHECK_EQ(reset.visible_entries[0], std::string("new"));
    HERMES_CHECK_EQ(reset.visible_entries[1], std::string("-"));
}
