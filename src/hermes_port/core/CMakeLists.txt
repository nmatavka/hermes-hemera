add_library(
    hermes_port_core
    src/BufferedMessageRenderer.cpp
    src/FilesystemPluginHost.cpp
    src/HunspellSpellService.cpp
    src/InMemoryWorkspaceModel.cpp
    src/IniSettingsStore.cpp
    src/MemoryRichTextSurface.cpp
    src/OpenSslTlsProvider.cpp
    src/RendererMode.cpp
)

add_library(hermes::port_core ALIAS hermes_port_core)

target_include_directories(
    hermes_port_core
    PUBLIC
        include
)

target_link_libraries(
    hermes_port_core
    PUBLIC
        hermes::dependencies
    PRIVATE
        ${CMAKE_DL_LIBS}
)

target_compile_features(hermes_port_core PUBLIC cxx_std_17)
