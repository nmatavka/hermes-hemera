if(TARGET hermes_port_dependencies)
    return()
endif()

add_library(hermes_port_dependencies INTERFACE)
add_library(hermes::dependencies ALIAS hermes_port_dependencies)

set(HERMES_THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party" CACHE PATH "Location of Haiku port third-party checkouts.")
set(HERMES_PAIGE_ROOT "${HERMES_THIRD_PARTY_DIR}/Hermes-Paige" CACHE PATH "Location of the Hermes-Paige checkout.")
set(HERMES_OPENSSL_ROOT "${HERMES_THIRD_PARTY_DIR}/openssl" CACHE PATH "Location of the OpenSSL 4.x checkout.")
set(HERMES_HUNSPELL_ROOT "${HERMES_THIRD_PARTY_DIR}/hunspell" CACHE PATH "Location of the Hunspell checkout.")

set(HERMES_IS_HAIKU 0)
if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    set(HERMES_IS_HAIKU 1)
endif()

set(HERMES_HAS_PAIGE 0)
if(EXISTS "${HERMES_PAIGE_ROOT}/PGHEADER/PAIGE.H")
    set(HERMES_HAS_PAIGE 1)
    target_include_directories(
        hermes_port_dependencies
        INTERFACE
            "${HERMES_PAIGE_ROOT}/PGHEADER"
            "${HERMES_PAIGE_ROOT}/CONTROL"
    )
endif()

set(HERMES_HAS_OPENSSL 0)
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    set(HERMES_HAS_OPENSSL 1)
    target_link_libraries(
        hermes_port_dependencies
        INTERFACE
            OpenSSL::SSL
            OpenSSL::Crypto
    )
elseif(EXISTS "${HERMES_OPENSSL_ROOT}/include/openssl/ssl.h")
    target_include_directories(
        hermes_port_dependencies
        INTERFACE
            "${HERMES_OPENSSL_ROOT}/include"
    )
endif()

set(HERMES_HAS_HUNSPELL 0)
find_path(
    HUNSPELL_INCLUDE_DIR
    NAMES hunspell.h hunspell.hxx
    HINTS
        "${HERMES_HUNSPELL_ROOT}/src/hunspell"
    PATH_SUFFIXES
        hunspell
)
find_library(
    HUNSPELL_LIBRARY
    NAMES hunspell-1.7 hunspell
)
if(HUNSPELL_INCLUDE_DIR AND HUNSPELL_LIBRARY)
    add_library(hunspell::hunspell UNKNOWN IMPORTED)
    set_target_properties(
        hunspell::hunspell
        PROPERTIES
            IMPORTED_LOCATION "${HUNSPELL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${HUNSPELL_INCLUDE_DIR}"
    )
    set(HERMES_HAS_HUNSPELL 1)
    target_link_libraries(
        hermes_port_dependencies
        INTERFACE
            hunspell::hunspell
    )
endif()

target_compile_definitions(
    hermes_port_dependencies
    INTERFACE
        HERMES_IS_HAIKU=${HERMES_IS_HAIKU}
        HERMES_HAS_PAIGE=${HERMES_HAS_PAIGE}
        HERMES_HAS_OPENSSL=${HERMES_HAS_OPENSSL}
        HERMES_HAS_HUNSPELL=${HERMES_HAS_HUNSPELL}
)

message(STATUS "Hermes Haiku Port: HAIKU=${HERMES_IS_HAIKU} PAIGE=${HERMES_HAS_PAIGE} OPENSSL=${HERMES_HAS_OPENSSL} HUNSPELL=${HERMES_HAS_HUNSPELL}")
