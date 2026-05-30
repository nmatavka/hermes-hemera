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
    GuiPreferencesTests.cpp
    ImportServiceTests.cpp
    TestMain.cpp
    RendererModeTests.cpp
    IniSettingsStoreTests.cpp
    ImapActionStoreTests.cpp
    MailStoreTests.cpp
    MailboxSortTests.cpp
    MailboxUiSettingsTests.cpp
    MailboxWorkflowTests.cpp
    MailTransportCoordinatorTests.cpp
    MoodWatchAnalyzerTests.cpp
    NicknameAndAddressBookTests.cpp
    OAuthSupportTests.cpp
    PluginHostTests.cpp
    ReleaseGateTests.cpp
    RichTextFormatTests.cpp
    SearchAndJunkTests.cpp
    ShellBehaviorAndCredentialTests.cpp
    SignatureStoreTests.cpp
    ToolbarConfigurationTests.cpp
    TransportServiceTests.cpp
    WorkflowToolStoreTests.cpp
    WorkspaceModelTests.cpp
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
        hermes::port_paige
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

if(HERMES_BUILD_HAIKU_SHELL AND CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    add_executable(
        hemera_haiku_compose_smoke
        HaikuComposeSmokeTest.cpp
    )

    target_include_directories(
        hemera_haiku_compose_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/src/hermes_port/haiku
    )

    target_link_libraries(
        hemera_haiku_compose_smoke
        PRIVATE
            hermes::legacy_transplants
            hermes::port_paige
            be
    )

    target_compile_definitions(
        hemera_haiku_compose_smoke
        PRIVATE
            HERMES_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME hemera_haiku_compose_smoke
        COMMAND hemera_haiku_compose_smoke
    )

    add_executable(
        hemera_haiku_workflow_smoke
        HaikuWorkflowSmokeTest.cpp
    )

    target_include_directories(
        hemera_haiku_workflow_smoke
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/src/hermes_port/haiku
    )

    target_link_libraries(
        hemera_haiku_workflow_smoke
        PRIVATE
            hermes::legacy_transplants
            hermes::port_paige
            be
    )

    target_compile_definitions(
        hemera_haiku_workflow_smoke
        PRIVATE
            HERMES_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    )

    add_test(
        NAME hemera_haiku_workflow_smoke
        COMMAND hemera_haiku_workflow_smoke
    )

    if(HERMES_HAS_HAIKU_WEBKIT)
        add_executable(
            hemera_haiku_webkit_smoke
            HaikuWebKitBuildSmoke.cpp
            ${CMAKE_SOURCE_DIR}/src/hermes_port/haiku/HaikuWebKitSupport.cpp
        )

        target_include_directories(
            hemera_haiku_webkit_smoke
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_SOURCE_DIR}/src/hermes_port/haiku
        )

        target_link_libraries(
            hemera_haiku_webkit_smoke
            PRIVATE
                hermes::port_paige
                hermes::haiku_webkit_legacy
                be
        )

        target_compile_definitions(
            hemera_haiku_webkit_smoke
            PRIVATE
                HERMES_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
        )

        add_test(
            NAME hemera_haiku_webkit_smoke
            COMMAND hemera_haiku_webkit_smoke
        )
    endif()

    if(HERMES_ENABLE_NATIVE_PAIGE)
        add_executable(
            hemera_haiku_native_paige_smoke
            HaikuNativePaigeBuildSmoke.cpp
        )

        target_include_directories(
            hemera_haiku_native_paige_smoke
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_SOURCE_DIR}/src/hermes_port/haiku
        )

        target_link_libraries(
            hemera_haiku_native_paige_smoke
            PRIVATE
                hermes::port_paige
                paige
        )

        target_compile_definitions(
            hemera_haiku_native_paige_smoke
            PRIVATE
                HERMES_ENABLE_NATIVE_PAIGE=$<BOOL:${HERMES_ENABLE_NATIVE_PAIGE}>
        )

        add_test(
            NAME hemera_haiku_native_paige_smoke
            COMMAND hemera_haiku_native_paige_smoke
        )
    endif()
endif()
