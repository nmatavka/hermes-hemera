add_library(
    hermes_legacy_transplants
    LegacyAccountService.cpp
    LegacyHelpCatalog.cpp
    LegacyImportService.cpp
)

add_library(hermes::legacy_transplants ALIAS hermes_legacy_transplants)

target_include_directories(
    hermes_legacy_transplants
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/hermes_port/core/include
)

target_link_libraries(
    hermes_legacy_transplants
    PUBLIC
        hermes::port_core
)

target_compile_features(hermes_legacy_transplants PUBLIC cxx_std_17)
