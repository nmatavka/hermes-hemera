add_library(
    hermes_port_paige
    PaigeRichTextSurface.cpp
    PaigeRuntime.cpp
)

add_library(hermes::port_paige ALIAS hermes_port_paige)

target_include_directories(
    hermes_port_paige
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/hermes_port/core/include
)

target_link_libraries(
    hermes_port_paige
    PUBLIC
        hermes::port_core
)

target_compile_features(hermes_port_paige PUBLIC cxx_std_17)

target_compile_definitions(
    hermes_port_paige
    PRIVATE
        HERMES_ENABLE_NATIVE_PAIGE=$<BOOL:${HERMES_ENABLE_NATIVE_PAIGE}>
)
