add_library(
    hermes_sample_status_plugin
    MODULE
    SampleStatusPlugin.cpp
)

set_target_properties(
    hermes_sample_status_plugin
    PROPERTIES
        PREFIX ""
)

target_include_directories(
    hermes_sample_status_plugin
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/hermes_port/core/include
)

add_executable(
    hermes_port_tests
    TestMain.cpp
    RendererModeTests.cpp
    IniSettingsStoreTests.cpp
    PluginHostTests.cpp
)

target_include_directories(
    hermes_port_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    hermes_port_tests
    PRIVATE
        hermes::port_core
)

target_compile_definitions(
    hermes_port_tests
    PRIVATE
        HERMES_SAMPLE_PLUGIN_PATH="$<TARGET_FILE:hermes_sample_status_plugin>"
)

add_test(
    NAME hermes_port_tests
    COMMAND hermes_port_tests
)
