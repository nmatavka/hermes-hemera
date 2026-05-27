add_library(
    hermes_port_core
    src/CredentialStore.cpp
    src/ComposeController.cpp
    src/ComposeMessage.cpp
    src/ComposeQueue.cpp
    src/FilesystemMailboxStore.cpp
    src/FilesystemDraftStore.cpp
    src/GssapiAuthenticator.cpp
    src/ImapActionStore.cpp
    src/BufferedMessageRenderer.cpp
    src/FilesystemMessageStore.cpp
    src/FilesystemPluginHost.cpp
    src/FilesystemSignatureStore.cpp
    src/FilesystemStationeryStore.cpp
    src/FlatFileNicknameStore.cpp
    src/HeuristicJunkScorer.cpp
    src/HunspellSpellService.cpp
    src/InMemoryWorkspaceModel.cpp
    src/IniSettingsStore.cpp
    src/MailTaskModel.cpp
    src/MailTransportCoordinator.cpp
    src/MemoryAddressBookService.cpp
    src/MemoryRichTextSurface.cpp
    src/OpenSslTlsProvider.cpp
    src/RendererMode.cpp
    src/RuleBasedFilterEngine.cpp
    src/SyncStateStore.cpp
    src/SimpleSearchService.cpp
    src/SocketTransportService.cpp
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
