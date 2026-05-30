if(TARGET hermes_port_dependencies)
    return()
endif()

include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

add_library(hermes_port_dependencies INTERFACE)
add_library(hermes::dependencies ALIAS hermes_port_dependencies)

set(HERMES_THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party" CACHE PATH "Location of optional pinned dependency checkouts.")
set(HERMES_DEPENDENCY_FETCH_ROOT "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "Location of build-tree dependency fetch checkouts.")
option(HERMES_PREFER_SYSTEM_DEPENDENCIES "Prefer system-installed dependency libraries before falling back to pinned source trees." ON)
option(HERMES_ENABLE_DEPENDENCY_FETCH "Automatically fetch pinned dependency source trees into the build directory when needed." ON)

set(_HERMES_PAIGE_ROOT_EXPLICIT 0)
if(DEFINED CACHE{HERMES_PAIGE_ROOT})
    set(_HERMES_PAIGE_ROOT_EXPLICIT 1)
endif()
set(_HERMES_OPENSSL_ROOT_EXPLICIT 0)
if(DEFINED CACHE{HERMES_OPENSSL_ROOT})
    set(_HERMES_OPENSSL_ROOT_EXPLICIT 1)
endif()
set(_HERMES_HUNSPELL_ROOT_EXPLICIT 0)
if(DEFINED CACHE{HERMES_HUNSPELL_ROOT})
    set(_HERMES_HUNSPELL_ROOT_EXPLICIT 1)
endif()
set(_HERMES_KRB5_ROOT_EXPLICIT 0)
if(DEFINED CACHE{HERMES_KRB5_ROOT})
    set(_HERMES_KRB5_ROOT_EXPLICIT 1)
endif()

set(HERMES_PAIGE_ROOT "${HERMES_THIRD_PARTY_DIR}/Hermes-Paige" CACHE PATH "Location of the Hermes-Paige checkout.")
set(HERMES_OPENSSL_ROOT "${HERMES_THIRD_PARTY_DIR}/openssl" CACHE PATH "Location of the OpenSSL checkout.")
set(HERMES_HUNSPELL_ROOT "${HERMES_THIRD_PARTY_DIR}/hunspell" CACHE PATH "Location of the Hunspell checkout.")
set(HERMES_KRB5_ROOT "${HERMES_THIRD_PARTY_DIR}/krb5" CACHE PATH "Location of the MIT Kerberos checkout.")

set(HERMES_IS_HAIKU 0)
if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    set(HERMES_IS_HAIKU 1)
endif()

set(_HEMERA_PAIGE_GIT_REPOSITORY "https://github.com/nmatavka/Hermes-Paige")
set(_HEMERA_PAIGE_GIT_TAG "367c01f1510304f90c7c944e2e356bebe8eef040")
set(_HEMERA_OPENSSL_GIT_REPOSITORY "https://github.com/openssl/openssl")
set(_HEMERA_OPENSSL_GIT_TAG "11b7b6ea3b65a584e1d31408ed1bdb139465cffd")
set(_HEMERA_HUNSPELL_GIT_REPOSITORY "https://github.com/hunspell/hunspell")
set(_HEMERA_HUNSPELL_GIT_TAG "c5f98152a274e25b5107101104bef632b83a0cc9")
set(_HEMERA_KRB5_GIT_REPOSITORY "https://github.com/krb5/krb5.git")
set(_HEMERA_KRB5_GIT_TAG "8570e77819563e036027e1da789d08ec9333ed4d")

function(hemera_fetch_dependency dependency_id repository git_tag output_root)
    if(NOT HERMES_ENABLE_DEPENDENCY_FETCH)
        set(${output_root} "" PARENT_SCOPE)
        return()
    endif()

    string(REPLACE "/" "_" content_name "hemera_dep_${dependency_id}")
    set(source_dir "${HERMES_DEPENDENCY_FETCH_ROOT}/src/${dependency_id}")
    set(subbuild_dir "${HERMES_DEPENDENCY_FETCH_ROOT}/subbuild/${dependency_id}")

    FetchContent_Declare(
        ${content_name}
        GIT_REPOSITORY "${repository}"
        GIT_TAG "${git_tag}"
        GIT_SHALLOW FALSE
        UPDATE_DISCONNECTED TRUE
        SOURCE_DIR "${source_dir}"
        SUBBUILD_DIR "${subbuild_dir}"
    )
    FetchContent_GetProperties(${content_name})
    if(NOT ${content_name}_POPULATED)
        message(STATUS "Hemera: fetching ${dependency_id} into ${source_dir}")
        FetchContent_Populate(${content_name})
    endif()

    set(${output_root} "${source_dir}" PARENT_SCOPE)
endfunction()

function(hemera_find_openssl_from_root root)
    find_path(
        _HERMES_OPENSSL_INCLUDE_DIR
        NAMES openssl/ssl.h
        HINTS
            "${root}/include"
        NO_DEFAULT_PATH
    )
    find_library(
        _HERMES_OPENSSL_SSL_LIBRARY
        NAMES ssl libssl
        HINTS
            "${root}/lib"
            "${root}/lib64"
            "${root}/build/lib"
            "${root}/build/src/lib"
        NO_DEFAULT_PATH
    )
    find_library(
        _HERMES_OPENSSL_CRYPTO_LIBRARY
        NAMES crypto libcrypto
        HINTS
            "${root}/lib"
            "${root}/lib64"
            "${root}/build/lib"
            "${root}/build/src/lib"
        NO_DEFAULT_PATH
    )

    set(HERMES_OPENSSL_INCLUDE_DIR "${_HERMES_OPENSSL_INCLUDE_DIR}" PARENT_SCOPE)
    set(HERMES_OPENSSL_SSL_LIBRARY "${_HERMES_OPENSSL_SSL_LIBRARY}" PARENT_SCOPE)
    set(HERMES_OPENSSL_CRYPTO_LIBRARY "${_HERMES_OPENSSL_CRYPTO_LIBRARY}" PARENT_SCOPE)
endfunction()

function(hemera_find_hunspell_from_root root)
    find_path(
        _HERMES_HUNSPELL_INCLUDE_DIR
        NAMES hunspell.h hunspell.hxx
        HINTS
            "${root}/src/hunspell"
        PATH_SUFFIXES
            hunspell
        NO_DEFAULT_PATH
    )
    find_library(
        _HERMES_HUNSPELL_LIBRARY
        NAMES hunspell-1.7 hunspell
        HINTS
            "${root}/lib"
            "${root}/lib64"
            "${root}/src/hunspell/.libs"
            "${root}/build/lib"
            "${root}/build/src/lib"
        NO_DEFAULT_PATH
    )

    set(HUNSPELL_INCLUDE_DIR "${_HERMES_HUNSPELL_INCLUDE_DIR}" PARENT_SCOPE)
    set(HUNSPELL_LIBRARY "${_HERMES_HUNSPELL_LIBRARY}" PARENT_SCOPE)
endfunction()

function(hemera_find_krb5_from_root root)
    find_path(
        _HERMES_KRB5_INCLUDE_DIR
        NAMES krb5.h
        HINTS
            "${root}/src/include"
            "${root}/include"
        NO_DEFAULT_PATH
    )
    find_path(
        _HERMES_GSSAPI_INCLUDE_DIR
        NAMES gssapi/gssapi.h
        HINTS
            "${root}/src/include"
            "${root}/include"
        NO_DEFAULT_PATH
    )
    find_library(
        _HERMES_GSSAPI_LIBRARY
        NAMES gssapi_krb5 gssapi
        HINTS
            "${root}/lib"
            "${root}/lib64"
            "${root}/src/lib"
            "${root}/build/lib"
            "${root}/build/src/lib"
        NO_DEFAULT_PATH
    )
    find_library(
        _HERMES_KRB5_LIBRARY
        NAMES krb5
        HINTS
            "${root}/lib"
            "${root}/lib64"
            "${root}/src/lib"
            "${root}/build/lib"
            "${root}/build/src/lib"
        NO_DEFAULT_PATH
    )
    find_library(
        _HERMES_K5CRYPTO_LIBRARY
        NAMES k5crypto
        HINTS
            "${root}/lib"
            "${root}/lib64"
            "${root}/src/lib"
            "${root}/build/lib"
            "${root}/build/src/lib"
        NO_DEFAULT_PATH
    )
    find_library(
        _HERMES_COMERR_LIBRARY
        NAMES com_err
        HINTS
            "${root}/lib"
            "${root}/lib64"
            "${root}/src/lib"
            "${root}/build/lib"
            "${root}/build/src/lib"
        NO_DEFAULT_PATH
    )

    set(HERMES_KRB5_INCLUDE_DIR "${_HERMES_KRB5_INCLUDE_DIR}" PARENT_SCOPE)
    set(HERMES_GSSAPI_INCLUDE_DIR "${_HERMES_GSSAPI_INCLUDE_DIR}" PARENT_SCOPE)
    set(HERMES_GSSAPI_LIBRARY "${_HERMES_GSSAPI_LIBRARY}" PARENT_SCOPE)
    set(HERMES_KRB5_LIBRARY "${_HERMES_KRB5_LIBRARY}" PARENT_SCOPE)
    set(HERMES_K5CRYPTO_LIBRARY "${_HERMES_K5CRYPTO_LIBRARY}" PARENT_SCOPE)
    set(HERMES_COMERR_LIBRARY "${_HERMES_COMERR_LIBRARY}" PARENT_SCOPE)
endfunction()

set(HERMES_HAS_PAIGE 0)
if(HERMES_ENABLE_NATIVE_PAIGE AND HERMES_IS_HAIKU AND NOT EXISTS "${HERMES_PAIGE_ROOT}/PGHEADER/PAIGE.H")
    hemera_fetch_dependency("Hermes-Paige" "${_HEMERA_PAIGE_GIT_REPOSITORY}" "${_HEMERA_PAIGE_GIT_TAG}" _HERMES_FETCHED_PAIGE_ROOT)
    if(EXISTS "${_HERMES_FETCHED_PAIGE_ROOT}/PGHEADER/PAIGE.H")
        set(HERMES_PAIGE_ROOT "${_HERMES_FETCHED_PAIGE_ROOT}" CACHE PATH "Location of the Hermes-Paige checkout." FORCE)
    endif()
endif()
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
unset(HERMES_OPENSSL_INCLUDE_DIR CACHE)
unset(HERMES_OPENSSL_SSL_LIBRARY CACHE)
unset(HERMES_OPENSSL_CRYPTO_LIBRARY CACHE)
if(_HERMES_OPENSSL_ROOT_EXPLICIT AND EXISTS "${HERMES_OPENSSL_ROOT}")
    hemera_find_openssl_from_root("${HERMES_OPENSSL_ROOT}")
endif()
if(NOT HERMES_OPENSSL_INCLUDE_DIR AND HERMES_PREFER_SYSTEM_DEPENDENCIES)
    find_package(OpenSSL QUIET)
endif()
if(OpenSSL_FOUND)
    set(HERMES_HAS_OPENSSL 1)
    target_link_libraries(
        hermes_port_dependencies
        INTERFACE
            OpenSSL::SSL
            OpenSSL::Crypto
    )
else()
    if(NOT HERMES_OPENSSL_INCLUDE_DIR AND EXISTS "${HERMES_OPENSSL_ROOT}")
        hemera_find_openssl_from_root("${HERMES_OPENSSL_ROOT}")
    endif()
    if(NOT HERMES_OPENSSL_INCLUDE_DIR)
        hemera_fetch_dependency("openssl" "${_HEMERA_OPENSSL_GIT_REPOSITORY}" "${_HEMERA_OPENSSL_GIT_TAG}" _HERMES_FETCHED_OPENSSL_ROOT)
        if(EXISTS "${_HERMES_FETCHED_OPENSSL_ROOT}/include/openssl/ssl.h")
            set(HERMES_OPENSSL_ROOT "${_HERMES_FETCHED_OPENSSL_ROOT}" CACHE PATH "Location of the OpenSSL checkout." FORCE)
            hemera_find_openssl_from_root("${HERMES_OPENSSL_ROOT}")
        endif()
    endif()
    if(NOT HERMES_OPENSSL_INCLUDE_DIR AND NOT HERMES_PREFER_SYSTEM_DEPENDENCIES)
        find_package(OpenSSL QUIET)
    endif()
    if(OpenSSL_FOUND)
        set(HERMES_HAS_OPENSSL 1)
        target_link_libraries(
            hermes_port_dependencies
            INTERFACE
                OpenSSL::SSL
                OpenSSL::Crypto
        )
    elseif(HERMES_OPENSSL_INCLUDE_DIR AND HERMES_OPENSSL_SSL_LIBRARY AND HERMES_OPENSSL_CRYPTO_LIBRARY)
        if(NOT TARGET hemera_openssl_ssl)
            add_library(hemera_openssl_ssl UNKNOWN IMPORTED)
            set_target_properties(
                hemera_openssl_ssl
                PROPERTIES
                    IMPORTED_LOCATION "${HERMES_OPENSSL_SSL_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${HERMES_OPENSSL_INCLUDE_DIR}"
            )
            add_library(OpenSSL::SSL ALIAS hemera_openssl_ssl)
        endif()
        if(NOT TARGET hemera_openssl_crypto)
            add_library(hemera_openssl_crypto UNKNOWN IMPORTED)
            set_target_properties(
                hemera_openssl_crypto
                PROPERTIES
                    IMPORTED_LOCATION "${HERMES_OPENSSL_CRYPTO_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${HERMES_OPENSSL_INCLUDE_DIR}"
            )
            add_library(OpenSSL::Crypto ALIAS hemera_openssl_crypto)
        endif()
        set(HERMES_HAS_OPENSSL 1)
        target_link_libraries(
            hermes_port_dependencies
            INTERFACE
                OpenSSL::SSL
                OpenSSL::Crypto
        )
    elseif(HERMES_OPENSSL_INCLUDE_DIR)
        target_include_directories(
            hermes_port_dependencies
            INTERFACE
                "${HERMES_OPENSSL_INCLUDE_DIR}"
        )
    endif()
endif()

set(HERMES_HAS_HUNSPELL 0)
set(HUNSPELL_INCLUDE_DIR "")
set(HUNSPELL_LIBRARY "")
if(_HERMES_HUNSPELL_ROOT_EXPLICIT AND EXISTS "${HERMES_HUNSPELL_ROOT}")
    hemera_find_hunspell_from_root("${HERMES_HUNSPELL_ROOT}")
endif()
if(NOT HUNSPELL_INCLUDE_DIR AND HERMES_PREFER_SYSTEM_DEPENDENCIES)
    find_path(
        HUNSPELL_INCLUDE_DIR
        NAMES hunspell.h hunspell.hxx
        PATH_SUFFIXES
            hunspell
    )
    find_library(
        HUNSPELL_LIBRARY
        NAMES hunspell-1.7 hunspell
    )
endif()
if(NOT HUNSPELL_INCLUDE_DIR AND EXISTS "${HERMES_HUNSPELL_ROOT}")
    hemera_find_hunspell_from_root("${HERMES_HUNSPELL_ROOT}")
endif()
if(HUNSPELL_INCLUDE_DIR AND NOT HUNSPELL_LIBRARY)
    find_library(
        HUNSPELL_LIBRARY
        NAMES hunspell-1.7 hunspell
    )
endif()
if(NOT HUNSPELL_INCLUDE_DIR)
    hemera_fetch_dependency("hunspell" "${_HEMERA_HUNSPELL_GIT_REPOSITORY}" "${_HEMERA_HUNSPELL_GIT_TAG}" _HERMES_FETCHED_HUNSPELL_ROOT)
    if(EXISTS "${_HERMES_FETCHED_HUNSPELL_ROOT}/src/hunspell")
        set(HERMES_HUNSPELL_ROOT "${_HERMES_FETCHED_HUNSPELL_ROOT}" CACHE PATH "Location of the Hunspell checkout." FORCE)
        hemera_find_hunspell_from_root("${HERMES_HUNSPELL_ROOT}")
    endif()
endif()
if(NOT HUNSPELL_INCLUDE_DIR AND NOT HERMES_PREFER_SYSTEM_DEPENDENCIES)
    find_path(
        HUNSPELL_INCLUDE_DIR
        NAMES hunspell.h hunspell.hxx
        PATH_SUFFIXES
            hunspell
    )
    find_library(
        HUNSPELL_LIBRARY
        NAMES hunspell-1.7 hunspell
    )
endif()
if(HUNSPELL_INCLUDE_DIR AND HUNSPELL_LIBRARY)
    if(NOT TARGET hunspell::hunspell)
        add_library(hunspell::hunspell UNKNOWN IMPORTED)
        set_target_properties(
            hunspell::hunspell
            PROPERTIES
                IMPORTED_LOCATION "${HUNSPELL_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${HUNSPELL_INCLUDE_DIR}"
        )
    endif()
    set(HERMES_HAS_HUNSPELL 1)
    target_link_libraries(
        hermes_port_dependencies
        INTERFACE
            hunspell::hunspell
    )
endif()

set(HERMES_HAS_KRB5 0)
set(HERMES_KRB5_HEADERS_FROM_ROOT 0)
set(HERMES_GSSAPI_LIBRARY_FROM_ROOT 0)
set(HERMES_KRB5_LIBRARY_FROM_ROOT 0)
set(_HERMES_KRB5_RESOLVED_ROOT_PATH "")
set(HERMES_KRB5_INCLUDE_DIR "")
set(HERMES_GSSAPI_INCLUDE_DIR "")
set(HERMES_GSSAPI_LIBRARY "")
set(HERMES_KRB5_LIBRARY "")
set(HERMES_K5CRYPTO_LIBRARY "")
set(HERMES_COMERR_LIBRARY "")

if(_HERMES_KRB5_ROOT_EXPLICIT AND EXISTS "${HERMES_KRB5_ROOT}")
    hemera_find_krb5_from_root("${HERMES_KRB5_ROOT}")
    if(HERMES_KRB5_INCLUDE_DIR)
        set(HERMES_KRB5_HEADERS_FROM_ROOT 1)
        set(_HERMES_KRB5_RESOLVED_ROOT_PATH "${HERMES_KRB5_ROOT}")
    endif()
    if(HERMES_GSSAPI_LIBRARY)
        set(HERMES_GSSAPI_LIBRARY_FROM_ROOT 1)
    endif()
    if(HERMES_KRB5_LIBRARY)
        set(HERMES_KRB5_LIBRARY_FROM_ROOT 1)
    endif()
endif()

if(NOT HERMES_KRB5_INCLUDE_DIR AND HERMES_PREFER_SYSTEM_DEPENDENCIES)
    find_path(HERMES_KRB5_INCLUDE_DIR NAMES krb5.h)
    find_path(HERMES_GSSAPI_INCLUDE_DIR NAMES gssapi/gssapi.h)
    find_library(HERMES_GSSAPI_LIBRARY NAMES gssapi_krb5 gssapi)
    find_library(HERMES_KRB5_LIBRARY NAMES krb5)
    find_library(HERMES_K5CRYPTO_LIBRARY NAMES k5crypto)
    find_library(HERMES_COMERR_LIBRARY NAMES com_err)
endif()

if(NOT HERMES_KRB5_INCLUDE_DIR AND EXISTS "${HERMES_KRB5_ROOT}")
    hemera_find_krb5_from_root("${HERMES_KRB5_ROOT}")
    if(HERMES_KRB5_INCLUDE_DIR)
        set(HERMES_KRB5_HEADERS_FROM_ROOT 1)
        set(_HERMES_KRB5_RESOLVED_ROOT_PATH "${HERMES_KRB5_ROOT}")
    endif()
    if(HERMES_GSSAPI_LIBRARY)
        set(HERMES_GSSAPI_LIBRARY_FROM_ROOT 1)
    endif()
    if(HERMES_KRB5_LIBRARY)
        set(HERMES_KRB5_LIBRARY_FROM_ROOT 1)
    endif()
endif()

if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_GSSAPI_INCLUDE_DIR)
    find_path(HERMES_GSSAPI_INCLUDE_DIR NAMES gssapi/gssapi.h)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_GSSAPI_LIBRARY)
    find_library(HERMES_GSSAPI_LIBRARY NAMES gssapi_krb5 gssapi)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_KRB5_LIBRARY)
    find_library(HERMES_KRB5_LIBRARY NAMES krb5)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_K5CRYPTO_LIBRARY)
    find_library(HERMES_K5CRYPTO_LIBRARY NAMES k5crypto)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_COMERR_LIBRARY)
    find_library(HERMES_COMERR_LIBRARY NAMES com_err)
endif()

if(NOT HERMES_KRB5_INCLUDE_DIR)
    hemera_fetch_dependency("krb5" "${_HEMERA_KRB5_GIT_REPOSITORY}" "${_HEMERA_KRB5_GIT_TAG}" _HERMES_FETCHED_KRB5_ROOT)
    if(EXISTS "${_HERMES_FETCHED_KRB5_ROOT}/src/include/krb5.h")
        set(HERMES_KRB5_ROOT "${_HERMES_FETCHED_KRB5_ROOT}" CACHE PATH "Location of the MIT Kerberos checkout." FORCE)
        hemera_find_krb5_from_root("${HERMES_KRB5_ROOT}")
        if(HERMES_KRB5_INCLUDE_DIR)
            set(HERMES_KRB5_HEADERS_FROM_ROOT 1)
            set(_HERMES_KRB5_RESOLVED_ROOT_PATH "${HERMES_KRB5_ROOT}")
        endif()
        if(HERMES_GSSAPI_LIBRARY)
            set(HERMES_GSSAPI_LIBRARY_FROM_ROOT 1)
        endif()
        if(HERMES_KRB5_LIBRARY)
            set(HERMES_KRB5_LIBRARY_FROM_ROOT 1)
        endif()
    endif()
endif()

if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_GSSAPI_INCLUDE_DIR)
    find_path(HERMES_GSSAPI_INCLUDE_DIR NAMES gssapi/gssapi.h)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_GSSAPI_LIBRARY)
    find_library(HERMES_GSSAPI_LIBRARY NAMES gssapi_krb5 gssapi)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_KRB5_LIBRARY)
    find_library(HERMES_KRB5_LIBRARY NAMES krb5)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_K5CRYPTO_LIBRARY)
    find_library(HERMES_K5CRYPTO_LIBRARY NAMES k5crypto)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_COMERR_LIBRARY)
    find_library(HERMES_COMERR_LIBRARY NAMES com_err)
endif()

if(NOT HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_PREFER_SYSTEM_DEPENDENCIES)
    find_path(HERMES_KRB5_INCLUDE_DIR NAMES krb5.h)
    find_path(HERMES_GSSAPI_INCLUDE_DIR NAMES gssapi/gssapi.h)
    find_library(HERMES_GSSAPI_LIBRARY NAMES gssapi_krb5 gssapi)
    find_library(HERMES_KRB5_LIBRARY NAMES krb5)
    find_library(HERMES_K5CRYPTO_LIBRARY NAMES k5crypto)
    find_library(HERMES_COMERR_LIBRARY NAMES com_err)
endif()

find_library(
    HERMES_GSS_FRAMEWORK
    NAMES GSS
)

if(HERMES_KRB5_INCLUDE_DIR AND HERMES_GSSAPI_INCLUDE_DIR AND HERMES_GSSAPI_LIBRARY)
    set(HERMES_HAS_KRB5 1)
    target_include_directories(
        hermes_port_dependencies
        INTERFACE
            "${HERMES_KRB5_INCLUDE_DIR}"
            "${HERMES_GSSAPI_INCLUDE_DIR}"
    )
    target_link_libraries(
        hermes_port_dependencies
        INTERFACE
            "${HERMES_GSSAPI_LIBRARY}"
    )
    if(HERMES_KRB5_LIBRARY)
        target_link_libraries(
            hermes_port_dependencies
            INTERFACE
                "${HERMES_KRB5_LIBRARY}"
        )
    endif()
    if(HERMES_K5CRYPTO_LIBRARY)
        target_link_libraries(
            hermes_port_dependencies
            INTERFACE
                "${HERMES_K5CRYPTO_LIBRARY}"
        )
    endif()
    if(HERMES_COMERR_LIBRARY)
        target_link_libraries(
            hermes_port_dependencies
            INTERFACE
                "${HERMES_COMERR_LIBRARY}"
        )
    endif()
elseif(HERMES_KRB5_INCLUDE_DIR AND HERMES_GSSAPI_INCLUDE_DIR AND HERMES_GSS_FRAMEWORK)
    set(HERMES_HAS_KRB5 1)
    target_include_directories(
        hermes_port_dependencies
        INTERFACE
            "${HERMES_KRB5_INCLUDE_DIR}"
            "${HERMES_GSSAPI_INCLUDE_DIR}"
    )
    target_link_libraries(
        hermes_port_dependencies
        INTERFACE
            "${HERMES_GSS_FRAMEWORK}"
    )
endif()

set(HERMES_HAS_HAIKU_WEBKIT 0)
if(HERMES_BUILD_HAIKU_SHELL AND HERMES_IS_HAIKU)
    find_path(
        HERMES_HAIKU_WEBKIT_INCLUDE_ROOT
        NAMES WebKitLegacy/haiku/API/WebView.h
        HINTS
            /boot/system/develop/headers
            /boot/home/config/develop/headers
    )
    find_library(
        HERMES_HAIKU_WEBKIT_LEGACY_LIBRARY
        NAMES WebKitLegacy
        HINTS
            /boot/system/lib
            /boot/home/config/lib
    )

    if(HERMES_HAIKU_WEBKIT_INCLUDE_ROOT AND HERMES_HAIKU_WEBKIT_LEGACY_LIBRARY)
        add_library(hermes_haiku_webkit_legacy INTERFACE)
        add_library(hermes::haiku_webkit_legacy ALIAS hermes_haiku_webkit_legacy)
        target_include_directories(
            hermes_haiku_webkit_legacy
            INTERFACE
                "${HERMES_HAIKU_WEBKIT_INCLUDE_ROOT}"
                "${HERMES_HAIKU_WEBKIT_INCLUDE_ROOT}/WebKitLegacy/haiku/API"
        )
        target_link_libraries(
            hermes_haiku_webkit_legacy
            INTERFACE
                "${HERMES_HAIKU_WEBKIT_LEGACY_LIBRARY}"
        )
        set(HERMES_HAS_HAIKU_WEBKIT 1)
    else()
        message(FATAL_ERROR
            "HERMES_BUILD_HAIKU_SHELL requires the Haiku WebKitLegacy system package. "
            "Missing WebKitLegacy headers or library.")
    endif()
endif()

target_compile_definitions(
    hermes_port_dependencies
    INTERFACE
        HERMES_IS_HAIKU=${HERMES_IS_HAIKU}
        HERMES_HAS_PAIGE=${HERMES_HAS_PAIGE}
        HERMES_HAS_OPENSSL=${HERMES_HAS_OPENSSL}
        HERMES_HAS_HUNSPELL=${HERMES_HAS_HUNSPELL}
        HERMES_HAS_KRB5=${HERMES_HAS_KRB5}
        HERMES_HAS_HAIKU_WEBKIT=${HERMES_HAS_HAIKU_WEBKIT}
        HERMES_KRB5_HEADERS_FROM_ROOT=${HERMES_KRB5_HEADERS_FROM_ROOT}
        HERMES_GSSAPI_LIBRARY_FROM_ROOT=${HERMES_GSSAPI_LIBRARY_FROM_ROOT}
        HERMES_KRB5_LIBRARY_FROM_ROOT=${HERMES_KRB5_LIBRARY_FROM_ROOT}
        HERMES_KRB5_RESOLVED_ROOT_PATH="${_HERMES_KRB5_RESOLVED_ROOT_PATH}"
)

message(
    STATUS
        "Hemera Haiku Port: HAIKU=${HERMES_IS_HAIKU} PAIGE=${HERMES_HAS_PAIGE} "
        "OPENSSL=${HERMES_HAS_OPENSSL} HUNSPELL=${HERMES_HAS_HUNSPELL} "
        "KRB5=${HERMES_HAS_KRB5} WEBKIT=${HERMES_HAS_HAIKU_WEBKIT}"
)
