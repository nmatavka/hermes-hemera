add_executable(
    hermes_hemera_haiku
    HaikuComposeWindow.cpp
    Main.cpp
    HaikuMainWindow.cpp
    HaikuShellHost.cpp
)

target_link_libraries(
    hermes_hemera_haiku
    PRIVATE
        hermes::port_core
        hermes::port_paige
)

target_include_directories(
    hermes_hemera_haiku
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
