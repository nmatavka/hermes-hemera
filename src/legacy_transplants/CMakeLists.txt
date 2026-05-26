add_library(
    hermes_legacy_transplants
    LegacyAccountService.cpp
    LegacyHelpCatalog.cpp
    LegacyImportService.cpp
    TaeMoodWatchAnalyzer.cpp
    tae/TAE.c
    tae/TAECommon.c
    tae/TAEDictionary.c
    tae/TAEText.c
)

add_library(hermes::legacy_transplants ALIAS hermes_legacy_transplants)

target_include_directories(
    hermes_legacy_transplants
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/hermes_port/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/tae
)

target_link_libraries(
    hermes_legacy_transplants
    PUBLIC
        hermes::port_core
)

target_compile_features(hermes_legacy_transplants PUBLIC cxx_std_17)
