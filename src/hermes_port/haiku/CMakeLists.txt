add_executable(
    hermes_hemera_haiku
    HaikuComposeWindow.cpp
    Main.cpp
    HaikuMainWindow.cpp
    HaikuShellHost.cpp
    PaigeEditorView.cpp
)

target_link_libraries(
    hermes_hemera_haiku
    PRIVATE
        hermes::port_core
        hermes::port_paige
        hermes::legacy_transplants
)

target_include_directories(
    hermes_hemera_haiku
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(
    hermes_hemera_haiku
    PRIVATE
        HERMES_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

if(HERMES_IS_HAIKU)
    target_link_libraries(
        hermes_hemera_haiku
        PRIVATE
            be
    )
endif()
