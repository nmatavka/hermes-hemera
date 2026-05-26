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
    AccountServiceTests.cpp
    ComposeControllerTests.cpp
    ComposeQueueTests.cpp
    ComposePolicyTests.cpp
    DraftStoreTests.cpp
    FilterEngineTests.cpp
    HelpCatalogTests.cpp
    ImportServiceTests.cpp
    TestMain.cpp
    RendererModeTests.cpp
    IniSettingsStoreTests.cpp
    MailStoreTests.cpp
    MoodWatchAnalyzerTests.cpp
    NicknameAndAddressBookTests.cpp
    PluginHostTests.cpp
    ReleaseGateTests.cpp
    SearchAndJunkTests.cpp
    SignatureStoreTests.cpp
    TransportServiceTests.cpp
)

target_include_directories(
    hermes_port_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    hermes_port_tests
    PRIVATE
        hermes::legacy_transplants
)

target_compile_definitions(
    hermes_port_tests
    PRIVATE
        HERMES_FIXTURE_ROOT="${CMAKE_SOURCE_DIR}/tests/fixtures/legacy"
        HERMES_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
        HERMES_SAMPLE_PLUGIN_PATH="$<TARGET_FILE:hermes_sample_status_plugin>"
)

add_test(
    NAME hermes_port_tests
    COMMAND hermes_port_tests
)

add_test(
    NAME hermes_no_legacy_path_references
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/check_no_legacy_paths.sh" "${CMAKE_SOURCE_DIR}"
)
