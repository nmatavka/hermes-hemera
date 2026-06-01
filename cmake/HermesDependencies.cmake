if(TARGET hermes_port_dependencies)
    return()
endif()

include(FetchContent)
include(ExternalProject)
find_package(Git QUIET)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

add_library(hermes_port_dependencies INTERFACE)
add_library(hermes::dependencies ALIAS hermes_port_dependencies)

set(HERMES_THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party" CACHE PATH "Location of optional pinned dependency checkouts.")
set(HERMES_DEPENDENCY_FETCH_ROOT "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "Location of build-tree dependency fetch checkouts.")
set(HERMES_DEPENDENCY_REF_FILE "${CMAKE_SOURCE_DIR}/cmake/HermesDependencyRefs.env" CACHE FILEPATH
    "Shared dependency ref manifest consumed by both CMake and bootstrap_dependencies.sh.")
option(HERMES_PREFER_SYSTEM_DEPENDENCIES "Prefer system-installed dependency libraries before falling back to pinned source trees." ON)
option(HERMES_ENABLE_DEPENDENCY_FETCH "Automatically fetch pinned dependency source trees into the build directory when needed." ON)
option(HERMES_BUILD_FETCHED_DEPENDENCIES "Build fetched dependency source trees into staged fallback libraries when system packages are unavailable." ON)

set(HERMES_HAIKU_WEBKIT_INCLUDE_ROOT "" CACHE PATH "Optional explicit Haiku WebKitLegacy header root.")
set(HERMES_HAIKU_WEBKIT_LIBRARY_ROOT "" CACHE PATH "Optional explicit Haiku WebKitLegacy library root.")

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

if(NOT EXISTS "${HERMES_DEPENDENCY_REF_FILE}")
    message(FATAL_ERROR
        "Hemera dependency ref manifest is missing: ${HERMES_DEPENDENCY_REF_FILE}")
endif()

file(STRINGS "${HERMES_DEPENDENCY_REF_FILE}" _HERMES_DEPENDENCY_REF_LINES)
foreach(_hemera_ref_line IN LISTS _HERMES_DEPENDENCY_REF_LINES)
    string(STRIP "${_hemera_ref_line}" _hemera_ref_line)
    if(_hemera_ref_line STREQUAL "" OR _hemera_ref_line MATCHES "^#")
        continue()
    endif()
    if(NOT _hemera_ref_line MATCHES "^([A-Za-z0-9_]+)=(.+)$")
        message(FATAL_ERROR
            "Malformed dependency ref line in ${HERMES_DEPENDENCY_REF_FILE}: ${_hemera_ref_line}")
    endif()
    set("${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
endforeach()

foreach(_hemera_required_ref
        HEMERA_DEP_HERMES_PAIGE_REPOSITORY
        HEMERA_DEP_HERMES_PAIGE_REF
        HEMERA_DEP_OPENSSL_REPOSITORY
        HEMERA_DEP_OPENSSL_REF
        HEMERA_DEP_HUNSPELL_REPOSITORY
        HEMERA_DEP_HUNSPELL_REF
        HEMERA_DEP_KRB5_REPOSITORY
        HEMERA_DEP_KRB5_REF)
    if(NOT DEFINED ${_hemera_required_ref} OR "${${_hemera_required_ref}}" STREQUAL "")
        message(FATAL_ERROR
            "Required dependency ref ${_hemera_required_ref} is missing from ${HERMES_DEPENDENCY_REF_FILE}")
    endif()
endforeach()

set(_HEMERA_PAIGE_GIT_REPOSITORY "${HEMERA_DEP_HERMES_PAIGE_REPOSITORY}")
set(_HEMERA_PAIGE_GIT_TAG "${HEMERA_DEP_HERMES_PAIGE_REF}")
set(_HEMERA_OPENSSL_GIT_REPOSITORY "${HEMERA_DEP_OPENSSL_REPOSITORY}")
set(_HEMERA_OPENSSL_GIT_TAG "${HEMERA_DEP_OPENSSL_REF}")
set(_HEMERA_HUNSPELL_GIT_REPOSITORY "${HEMERA_DEP_HUNSPELL_REPOSITORY}")
set(_HEMERA_HUNSPELL_GIT_TAG "${HEMERA_DEP_HUNSPELL_REF}")
set(_HEMERA_KRB5_GIT_REPOSITORY "${HEMERA_DEP_KRB5_REPOSITORY}")
set(_HEMERA_KRB5_GIT_TAG "${HEMERA_DEP_KRB5_REF}")

set(_HERMES_PAIGE_PATCH_DIR "${CMAKE_SOURCE_DIR}/cmake/patches/hermes-paige")

find_program(HERMES_NATIVE_MAKE_PROGRAM NAMES gmake make)
find_program(HERMES_AUTORECONF_EXECUTABLE NAMES autoreconf)

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

function(hemera_apply_patchset source_dir patch_dir)
    if(NOT EXISTS "${patch_dir}")
        return()
    endif()
    if(NOT GIT_FOUND)
        message(FATAL_ERROR
            "Applying Hemera dependency patchsets requires Git. Missing while patching ${source_dir}.")
    endif()

    file(GLOB _hemera_patch_files LIST_DIRECTORIES FALSE "${patch_dir}/*.patch")
    foreach(_hemera_patch IN LISTS _hemera_patch_files)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" apply --reverse --check "${_hemera_patch}"
            RESULT_VARIABLE _hemera_patch_already_applied
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(_hemera_patch_already_applied EQUAL 0)
            continue()
        endif()

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" apply --check "${_hemera_patch}"
            RESULT_VARIABLE _hemera_patch_check_result
            OUTPUT_QUIET
            ERROR_VARIABLE _hemera_patch_check_error
        )
        if(NOT _hemera_patch_check_result EQUAL 0)
            message(FATAL_ERROR
                "Hemera could not apply dependency patch ${_hemera_patch} in ${source_dir}: ${_hemera_patch_check_error}")
        endif()

        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" apply "${_hemera_patch}"
            RESULT_VARIABLE _hemera_patch_apply_result
            OUTPUT_QUIET
            ERROR_VARIABLE _hemera_patch_apply_error
        )
        if(NOT _hemera_patch_apply_result EQUAL 0)
            message(FATAL_ERROR
                "Hemera failed to apply dependency patch ${_hemera_patch} in ${source_dir}: ${_hemera_patch_apply_error}")
        endif()
    endforeach()
endfunction()

function(hemera_paige_checkout_usable root output_flag)
    set(_hemera_paige_usable 0)
    if(EXISTS "${root}/PGHEADER/PAIGE.H"
       AND EXISTS "${root}/PGPLATFO/PGHAIKU.CPP"
       AND EXISTS "${root}/PGPLATFO/PGMODERN.CPP"
       AND EXISTS "${root}/PGPLATFO/PGMEMMGR_MODERN.CPP"
       AND EXISTS "${root}/CMakeLists.txt")
        set(_hemera_paige_usable 1)
    endif()
    set(${output_flag} "${_hemera_paige_usable}" PARENT_SCOPE)
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
        NAMES hunspell/hunspell.h hunspell/hunspell.hxx
        HINTS
            "${root}/include"
        NO_DEFAULT_PATH
    )
    if(NOT _HERMES_HUNSPELL_INCLUDE_DIR)
        find_path(
            _HERMES_HUNSPELL_INCLUDE_DIR
            NAMES hunspell.h hunspell.hxx
            HINTS
                "${root}/src/hunspell"
                "${root}/include/hunspell"
            NO_DEFAULT_PATH
        )
    endif()
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
    set(_hemera_krb5_include_hints "${root}/include")
    set(_hemera_krb5_library_hints "${root}/lib" "${root}/lib64")
    if(HERMES_IS_HAIKU)
        list(APPEND _hemera_krb5_include_hints "${root}/src/include")
        list(APPEND _hemera_krb5_library_hints
            "${root}/src/lib"
            "${root}/build/lib"
            "${root}/build/src/lib")
    endif()

    find_path(
        _HERMES_KRB5_INCLUDE_DIR
        NAMES krb5.h
        HINTS ${_hemera_krb5_include_hints}
        NO_DEFAULT_PATH
    )
    find_path(
        _HERMES_GSSAPI_INCLUDE_DIR
        NAMES gssapi/gssapi.h
        HINTS ${_hemera_krb5_include_hints}
        NO_DEFAULT_PATH
    )
    find_library(
        _HERMES_GSSAPI_LIBRARY
        NAMES gssapi_krb5 gssapi
        HINTS ${_hemera_krb5_library_hints}
        NO_DEFAULT_PATH
    )
    find_library(
        _HERMES_KRB5_LIBRARY
        NAMES krb5
        HINTS ${_hemera_krb5_library_hints}
        NO_DEFAULT_PATH
    )
    find_library(
        _HERMES_K5CRYPTO_LIBRARY
        NAMES k5crypto
        HINTS ${_hemera_krb5_library_hints}
        NO_DEFAULT_PATH
    )
    find_library(
        _HERMES_COMERR_LIBRARY
        NAMES com_err
        HINTS ${_hemera_krb5_library_hints}
        NO_DEFAULT_PATH
    )

    set(HERMES_KRB5_INCLUDE_DIR "${_HERMES_KRB5_INCLUDE_DIR}" PARENT_SCOPE)
    set(HERMES_GSSAPI_INCLUDE_DIR "${_HERMES_GSSAPI_INCLUDE_DIR}" PARENT_SCOPE)
    set(HERMES_GSSAPI_LIBRARY "${_HERMES_GSSAPI_LIBRARY}" PARENT_SCOPE)
    set(HERMES_KRB5_LIBRARY "${_HERMES_KRB5_LIBRARY}" PARENT_SCOPE)
    set(HERMES_K5CRYPTO_LIBRARY "${_HERMES_K5CRYPTO_LIBRARY}" PARENT_SCOPE)
    set(HERMES_COMERR_LIBRARY "${_HERMES_COMERR_LIBRARY}" PARENT_SCOPE)
endfunction()

function(hemera_stage_openssl_from_root root)
    if(NOT HERMES_BUILD_FETCHED_DEPENDENCIES)
        return()
    endif()
    if(NOT HERMES_NATIVE_MAKE_PROGRAM)
        message(FATAL_ERROR
            "Hemera needs make or gmake to build fetched OpenSSL from ${root}.")
    endif()

    set(_hemera_prefix "${HERMES_DEPENDENCY_FETCH_ROOT}/install/openssl")
    if(NOT TARGET hemera_dep_openssl_stage)
        ExternalProject_Add(
            hemera_dep_openssl_stage
            SOURCE_DIR "${root}"
            BUILD_IN_SOURCE TRUE
            CONFIGURE_COMMAND "${root}/config" no-shared no-tests --prefix=${_hemera_prefix} --openssldir=${_hemera_prefix}
            BUILD_COMMAND "${HERMES_NATIVE_MAKE_PROGRAM}" -j1
            INSTALL_COMMAND "${HERMES_NATIVE_MAKE_PROGRAM}" install_sw
            UPDATE_COMMAND ""
            BUILD_BYPRODUCTS
                "${_hemera_prefix}/lib/libssl${CMAKE_STATIC_LIBRARY_SUFFIX}"
                "${_hemera_prefix}/lib/libcrypto${CMAKE_STATIC_LIBRARY_SUFFIX}"
        )
    endif()

    if(NOT TARGET hemera_openssl_ssl)
        add_library(hemera_openssl_ssl INTERFACE)
        add_library(OpenSSL::SSL ALIAS hemera_openssl_ssl)
        add_dependencies(hemera_openssl_ssl hemera_dep_openssl_stage)
        target_include_directories(hemera_openssl_ssl INTERFACE "${_hemera_prefix}/include")
        target_link_directories(hemera_openssl_ssl INTERFACE "${_hemera_prefix}/lib")
        target_link_libraries(hemera_openssl_ssl INTERFACE ssl)
    endif()
    if(NOT TARGET hemera_openssl_crypto)
        add_library(hemera_openssl_crypto INTERFACE)
        add_library(OpenSSL::Crypto ALIAS hemera_openssl_crypto)
        add_dependencies(hemera_openssl_crypto hemera_dep_openssl_stage)
        target_include_directories(hemera_openssl_crypto INTERFACE "${_hemera_prefix}/include")
        target_link_directories(hemera_openssl_crypto INTERFACE "${_hemera_prefix}/lib")
        target_link_libraries(hemera_openssl_crypto INTERFACE crypto)
    endif()

    set(HERMES_OPENSSL_INCLUDE_DIR "${_hemera_prefix}/include" PARENT_SCOPE)
    set(HERMES_OPENSSL_SSL_LIBRARY "ssl" PARENT_SCOPE)
    set(HERMES_OPENSSL_CRYPTO_LIBRARY "crypto" PARENT_SCOPE)
    set(HERMES_OPENSSL_STAGED 1 PARENT_SCOPE)
endfunction()

function(hemera_stage_hunspell_from_root root)
    if(NOT HERMES_BUILD_FETCHED_DEPENDENCIES)
        return()
    endif()
    if(NOT HERMES_NATIVE_MAKE_PROGRAM)
        message(FATAL_ERROR
            "Hemera needs make or gmake to build fetched Hunspell from ${root}.")
    endif()
    if(NOT HERMES_AUTORECONF_EXECUTABLE)
        message(FATAL_ERROR
            "Hemera needs autoreconf to build fetched Hunspell from ${root}.")
    endif()

    set(_hemera_prefix "${HERMES_DEPENDENCY_FETCH_ROOT}/install/hunspell")
    if(NOT TARGET hemera_dep_hunspell_stage)
        ExternalProject_Add(
            hemera_dep_hunspell_stage
            SOURCE_DIR "${root}"
            BUILD_IN_SOURCE TRUE
            CONFIGURE_COMMAND sh -c "\"${HERMES_AUTORECONF_EXECUTABLE}\" -fi && ./configure --prefix=${_hemera_prefix} --disable-shared --enable-static"
            BUILD_COMMAND "${HERMES_NATIVE_MAKE_PROGRAM}" -j1
            INSTALL_COMMAND "${HERMES_NATIVE_MAKE_PROGRAM}" install
            UPDATE_COMMAND ""
            BUILD_BYPRODUCTS
                "${_hemera_prefix}/lib/libhunspell-1.7${CMAKE_STATIC_LIBRARY_SUFFIX}"
        )
    endif()

    if(NOT TARGET hemera_hunspell_fallback)
        add_library(hemera_hunspell_fallback INTERFACE)
        add_library(hunspell::hunspell ALIAS hemera_hunspell_fallback)
        add_dependencies(hemera_hunspell_fallback hemera_dep_hunspell_stage)
        target_include_directories(
            hemera_hunspell_fallback
            INTERFACE
                "${_hemera_prefix}/include"
                "${_hemera_prefix}/include/hunspell"
        )
        target_link_directories(hemera_hunspell_fallback INTERFACE "${_hemera_prefix}/lib")
        target_link_libraries(hemera_hunspell_fallback INTERFACE hunspell-1.7)
    endif()

    set(HUNSPELL_INCLUDE_DIR "${_hemera_prefix}/include" PARENT_SCOPE)
    set(HUNSPELL_LIBRARY "hunspell-1.7" PARENT_SCOPE)
    set(HERMES_HUNSPELL_STAGED 1 PARENT_SCOPE)
endfunction()

function(hemera_stage_krb5_from_root root)
    if(NOT HERMES_BUILD_FETCHED_DEPENDENCIES)
        return()
    endif()
    if(NOT HERMES_NATIVE_MAKE_PROGRAM)
        message(FATAL_ERROR
            "Hemera needs make or gmake to build fetched krb5 from ${root}.")
    endif()
    if(NOT HERMES_AUTORECONF_EXECUTABLE)
        message(FATAL_ERROR
            "Hemera needs autoreconf to build fetched krb5 from ${root}.")
    endif()

    set(_hemera_prefix "${HERMES_DEPENDENCY_FETCH_ROOT}/install/krb5")
    if(NOT TARGET hemera_dep_krb5_stage)
        ExternalProject_Add(
            hemera_dep_krb5_stage
            SOURCE_DIR "${root}/src"
            BUILD_IN_SOURCE TRUE
            CONFIGURE_COMMAND sh -c "\"${HERMES_AUTORECONF_EXECUTABLE}\" -fi && ./configure --prefix=${_hemera_prefix} --disable-shared --enable-static --without-system-verto"
            BUILD_COMMAND sh -c "\"${HERMES_NATIVE_MAKE_PROGRAM}\" -C include all-unix && \"${HERMES_NATIVE_MAKE_PROGRAM}\" -C util all-recurse && \"${HERMES_NATIVE_MAKE_PROGRAM}\" -C lib all-recurse"
            INSTALL_COMMAND sh -c "\"${HERMES_NATIVE_MAKE_PROGRAM}\" -C include install-headers && \"${HERMES_NATIVE_MAKE_PROGRAM}\" -C util install-unix && \"${HERMES_NATIVE_MAKE_PROGRAM}\" -C lib install-unix"
            UPDATE_COMMAND ""
            BUILD_BYPRODUCTS
                "${_hemera_prefix}/lib/libgssapi_krb5${CMAKE_STATIC_LIBRARY_SUFFIX}"
                "${_hemera_prefix}/lib/libkrb5${CMAKE_STATIC_LIBRARY_SUFFIX}"
                "${_hemera_prefix}/lib/libk5crypto${CMAKE_STATIC_LIBRARY_SUFFIX}"
                "${_hemera_prefix}/lib/libcom_err${CMAKE_STATIC_LIBRARY_SUFFIX}"
        )
    endif()

    if(NOT TARGET hemera_krb5_fallback)
        add_library(hemera_krb5_fallback INTERFACE)
        add_dependencies(hemera_krb5_fallback hemera_dep_krb5_stage)
        target_include_directories(
            hemera_krb5_fallback
            INTERFACE
                "${_hemera_prefix}/include"
        )
        target_link_directories(hemera_krb5_fallback INTERFACE "${_hemera_prefix}/lib")
        target_link_libraries(
            hemera_krb5_fallback
            INTERFACE
                gssapi_krb5
                krb5
                k5crypto
                com_err
        )
    endif()

    set(HERMES_KRB5_INCLUDE_DIR "${_hemera_prefix}/include" PARENT_SCOPE)
    set(HERMES_GSSAPI_INCLUDE_DIR "${_hemera_prefix}/include" PARENT_SCOPE)
    set(HERMES_GSSAPI_LIBRARY "gssapi_krb5" PARENT_SCOPE)
    set(HERMES_KRB5_LIBRARY "krb5" PARENT_SCOPE)
    set(HERMES_K5CRYPTO_LIBRARY "k5crypto" PARENT_SCOPE)
    set(HERMES_COMERR_LIBRARY "com_err" PARENT_SCOPE)
    set(HERMES_KRB5_STAGED 1 PARENT_SCOPE)
endfunction()

set(HERMES_HAS_PAIGE 0)
if(HERMES_ENABLE_NATIVE_PAIGE AND HERMES_IS_HAIKU)
    hemera_paige_checkout_usable("${HERMES_PAIGE_ROOT}" _HERMES_PAIGE_ROOT_USABLE)
    if(_HERMES_PAIGE_ROOT_EXPLICIT AND EXISTS "${HERMES_PAIGE_ROOT}" AND NOT _HERMES_PAIGE_ROOT_USABLE)
        message(FATAL_ERROR
            "Explicit HERMES_PAIGE_ROOT (${HERMES_PAIGE_ROOT}) does not contain a modern Haiku-capable Hermes-Paige checkout.")
    endif()
    if(NOT _HERMES_PAIGE_ROOT_USABLE)
        hemera_fetch_dependency("Hermes-Paige" "${_HEMERA_PAIGE_GIT_REPOSITORY}" "${_HEMERA_PAIGE_GIT_TAG}" _HERMES_FETCHED_PAIGE_ROOT)
        if(_HERMES_FETCHED_PAIGE_ROOT)
            set(HERMES_PAIGE_ROOT "${_HERMES_FETCHED_PAIGE_ROOT}" CACHE PATH "Location of the Hermes-Paige checkout." FORCE)
            hemera_paige_checkout_usable("${HERMES_PAIGE_ROOT}" _HERMES_PAIGE_ROOT_USABLE)
        endif()
    endif()
    if(_HERMES_PAIGE_ROOT_USABLE)
        hemera_apply_patchset("${HERMES_PAIGE_ROOT}" "${_HERMES_PAIGE_PATCH_DIR}")
        hemera_paige_checkout_usable("${HERMES_PAIGE_ROOT}" _HERMES_PAIGE_ROOT_USABLE)
    endif()
    if(NOT _HERMES_PAIGE_ROOT_USABLE)
        message(FATAL_ERROR
            "HERMES_ENABLE_NATIVE_PAIGE=ON on Haiku requires a usable Hermes-Paige checkout. "
            "Tried ${HERMES_PAIGE_ROOT} and fetch ref ${_HEMERA_PAIGE_GIT_TAG}.")
    endif()
    set(HERMES_HAS_PAIGE 1)
    target_include_directories(
        hermes_port_dependencies
        INTERFACE
            "${HERMES_PAIGE_ROOT}/PGHEADER"
            "${HERMES_PAIGE_ROOT}/CONTROL"
    )
endif()

set(HERMES_HAS_OPENSSL 0)
set(HERMES_OPENSSL_STAGED 0)
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
    if((NOT HERMES_OPENSSL_INCLUDE_DIR OR NOT HERMES_OPENSSL_SSL_LIBRARY OR NOT HERMES_OPENSSL_CRYPTO_LIBRARY)
       AND EXISTS "${HERMES_OPENSSL_ROOT}/Configure")
        hemera_stage_openssl_from_root("${HERMES_OPENSSL_ROOT}")
    endif()
    if(NOT HERMES_OPENSSL_INCLUDE_DIR)
        hemera_fetch_dependency("openssl" "${_HEMERA_OPENSSL_GIT_REPOSITORY}" "${_HEMERA_OPENSSL_GIT_TAG}" _HERMES_FETCHED_OPENSSL_ROOT)
        if(EXISTS "${_HERMES_FETCHED_OPENSSL_ROOT}/Configure")
            set(HERMES_OPENSSL_ROOT "${_HERMES_FETCHED_OPENSSL_ROOT}" CACHE PATH "Location of the OpenSSL checkout." FORCE)
            hemera_stage_openssl_from_root("${HERMES_OPENSSL_ROOT}")
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
    elseif(HERMES_OPENSSL_INCLUDE_DIR AND HERMES_OPENSSL_STAGED)
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
set(HERMES_HUNSPELL_STAGED 0)
set(HUNSPELL_INCLUDE_DIR "")
set(HUNSPELL_LIBRARY "")
if(_HERMES_HUNSPELL_ROOT_EXPLICIT AND EXISTS "${HERMES_HUNSPELL_ROOT}")
    hemera_find_hunspell_from_root("${HERMES_HUNSPELL_ROOT}")
endif()
if(NOT HUNSPELL_INCLUDE_DIR AND HERMES_PREFER_SYSTEM_DEPENDENCIES)
    find_path(
        HUNSPELL_INCLUDE_DIR
        NAMES hunspell/hunspell.h hunspell/hunspell.hxx
        PATH_SUFFIXES
            ""
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
if((NOT HUNSPELL_INCLUDE_DIR OR NOT HUNSPELL_LIBRARY)
   AND EXISTS "${HERMES_HUNSPELL_ROOT}/src/hunspell/hunspell.hxx")
    hemera_stage_hunspell_from_root("${HERMES_HUNSPELL_ROOT}")
endif()
if(HUNSPELL_INCLUDE_DIR AND NOT HUNSPELL_LIBRARY AND NOT HERMES_HUNSPELL_STAGED)
    find_library(
        HUNSPELL_LIBRARY
        NAMES hunspell-1.7 hunspell
    )
endif()
if(NOT HUNSPELL_INCLUDE_DIR)
    hemera_fetch_dependency("hunspell" "${_HEMERA_HUNSPELL_GIT_REPOSITORY}" "${_HEMERA_HUNSPELL_GIT_TAG}" _HERMES_FETCHED_HUNSPELL_ROOT)
    if(EXISTS "${_HERMES_FETCHED_HUNSPELL_ROOT}/src/hunspell/hunspell.hxx")
        set(HERMES_HUNSPELL_ROOT "${_HERMES_FETCHED_HUNSPELL_ROOT}" CACHE PATH "Location of the Hunspell checkout." FORCE)
        hemera_stage_hunspell_from_root("${HERMES_HUNSPELL_ROOT}")
    endif()
endif()
if(NOT HUNSPELL_INCLUDE_DIR AND NOT HERMES_PREFER_SYSTEM_DEPENDENCIES)
    find_path(
        HUNSPELL_INCLUDE_DIR
        NAMES hunspell/hunspell.h hunspell/hunspell.hxx
        PATH_SUFFIXES
            ""
            hunspell
    )
    find_library(
        HUNSPELL_LIBRARY
        NAMES hunspell-1.7 hunspell
    )
endif()
if(HUNSPELL_INCLUDE_DIR AND HERMES_HUNSPELL_STAGED)
    set(HERMES_HAS_HUNSPELL 1)
    target_link_libraries(
        hermes_port_dependencies
        INTERFACE
            hunspell::hunspell
    )
elseif(HUNSPELL_INCLUDE_DIR AND HUNSPELL_LIBRARY)
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
set(HERMES_KRB5_STAGED 0)
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
    if(HERMES_KRB5_INCLUDE_DIR OR EXISTS "${HERMES_KRB5_ROOT}/src/include/krb5.h")
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

if(NOT HERMES_KRB5_INCLUDE_DIR AND (HERMES_IS_HAIKU OR _HERMES_KRB5_ROOT_EXPLICIT) AND EXISTS "${HERMES_KRB5_ROOT}")
    hemera_find_krb5_from_root("${HERMES_KRB5_ROOT}")
    if(HERMES_KRB5_INCLUDE_DIR OR EXISTS "${HERMES_KRB5_ROOT}/src/include/krb5.h")
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

if(HERMES_IS_HAIKU
   AND (NOT HERMES_KRB5_INCLUDE_DIR OR NOT HERMES_GSSAPI_LIBRARY OR NOT HERMES_KRB5_LIBRARY)
   AND EXISTS "${HERMES_KRB5_ROOT}/src/include/krb5.h")
    hemera_stage_krb5_from_root("${HERMES_KRB5_ROOT}")
    set(HERMES_KRB5_HEADERS_FROM_ROOT 1)
    set(HERMES_GSSAPI_LIBRARY_FROM_ROOT 1)
    set(HERMES_KRB5_LIBRARY_FROM_ROOT 1)
    set(_HERMES_KRB5_RESOLVED_ROOT_PATH "${HERMES_KRB5_ROOT}")
endif()

if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_GSSAPI_INCLUDE_DIR)
    find_path(HERMES_GSSAPI_INCLUDE_DIR NAMES gssapi/gssapi.h)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_GSSAPI_LIBRARY AND NOT HERMES_KRB5_STAGED)
    find_library(HERMES_GSSAPI_LIBRARY NAMES gssapi_krb5 gssapi)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_KRB5_LIBRARY AND NOT HERMES_KRB5_STAGED)
    find_library(HERMES_KRB5_LIBRARY NAMES krb5)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_K5CRYPTO_LIBRARY AND NOT HERMES_KRB5_STAGED)
    find_library(HERMES_K5CRYPTO_LIBRARY NAMES k5crypto)
endif()
if(HERMES_KRB5_INCLUDE_DIR AND NOT HERMES_COMERR_LIBRARY AND NOT HERMES_KRB5_STAGED)
    find_library(HERMES_COMERR_LIBRARY NAMES com_err)
endif()

if(NOT HERMES_KRB5_INCLUDE_DIR AND HERMES_IS_HAIKU)
    hemera_fetch_dependency("krb5" "${_HEMERA_KRB5_GIT_REPOSITORY}" "${_HEMERA_KRB5_GIT_TAG}" _HERMES_FETCHED_KRB5_ROOT)
    if(EXISTS "${_HERMES_FETCHED_KRB5_ROOT}/src/include/krb5.h")
        set(HERMES_KRB5_ROOT "${_HERMES_FETCHED_KRB5_ROOT}" CACHE PATH "Location of the MIT Kerberos checkout." FORCE)
        hemera_stage_krb5_from_root("${HERMES_KRB5_ROOT}")
        set(HERMES_KRB5_HEADERS_FROM_ROOT 1)
        set(HERMES_GSSAPI_LIBRARY_FROM_ROOT 1)
        set(HERMES_KRB5_LIBRARY_FROM_ROOT 1)
        set(_HERMES_KRB5_RESOLVED_ROOT_PATH "${HERMES_KRB5_ROOT}")
    endif()
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

if(HERMES_KRB5_INCLUDE_DIR AND HERMES_KRB5_STAGED)
    set(HERMES_HAS_KRB5 1)
    target_link_libraries(
        hermes_port_dependencies
        INTERFACE
            hemera_krb5_fallback
    )
elseif(HERMES_KRB5_INCLUDE_DIR AND HERMES_GSSAPI_INCLUDE_DIR AND HERMES_GSSAPI_LIBRARY)
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
    set(_HERMES_HAIKU_WEBKIT_HEADER_ROOT_HINTS)
    if(HERMES_HAIKU_WEBKIT_INCLUDE_ROOT)
        list(APPEND _HERMES_HAIKU_WEBKIT_HEADER_ROOT_HINTS "${HERMES_HAIKU_WEBKIT_INCLUDE_ROOT}")
    endif()
    list(APPEND _HERMES_HAIKU_WEBKIT_HEADER_ROOT_HINTS
        /boot/system/develop/headers
        /boot/home/config/develop/headers
        /boot/system/non-packaged/develop/headers
        /boot/home/config/non-packaged/develop/headers
    )
    set(_HERMES_HAIKU_WEBKIT_LIBRARY_ROOT_HINTS)
    if(HERMES_HAIKU_WEBKIT_LIBRARY_ROOT)
        list(APPEND _HERMES_HAIKU_WEBKIT_LIBRARY_ROOT_HINTS "${HERMES_HAIKU_WEBKIT_LIBRARY_ROOT}")
    endif()
    list(APPEND _HERMES_HAIKU_WEBKIT_LIBRARY_ROOT_HINTS
        /boot/system/lib
        /boot/home/config/lib
        /boot/system/non-packaged/lib
        /boot/home/config/non-packaged/lib
    )

    find_path(
        HERMES_HAIKU_WEBKIT_INCLUDE_ROOT
        NAMES JavaScriptCore/JavaScript.h
        HINTS ${_HERMES_HAIKU_WEBKIT_HEADER_ROOT_HINTS}
        NO_DEFAULT_PATH
    )
    if(NOT HERMES_HAIKU_WEBKIT_INCLUDE_ROOT)
        find_path(
            HERMES_HAIKU_WEBKIT_INCLUDE_ROOT
            NAMES WebKitLegacy/haiku/API/WebView.h
            HINTS ${_HERMES_HAIKU_WEBKIT_HEADER_ROOT_HINTS}
            NO_DEFAULT_PATH
        )
    endif()
    find_path(
        HERMES_HAIKU_WEBKIT_API_INCLUDE_DIR
        NAMES WebView.h
        HINTS
            ${_HERMES_HAIKU_WEBKIT_HEADER_ROOT_HINTS}
            "${HERMES_HAIKU_WEBKIT_INCLUDE_ROOT}"
        PATH_SUFFIXES
            ""
            WebKitLegacy/haiku/API
        NO_DEFAULT_PATH
    )
    if(NOT HERMES_HAIKU_WEBKIT_API_INCLUDE_DIR
       AND HERMES_HAIKU_WEBKIT_INCLUDE_ROOT
       AND EXISTS "${HERMES_HAIKU_WEBKIT_INCLUDE_ROOT}/WebKitLegacy/haiku/API/WebView.h")
        set(HERMES_HAIKU_WEBKIT_API_INCLUDE_DIR
            "${HERMES_HAIKU_WEBKIT_INCLUDE_ROOT}/WebKitLegacy/haiku/API")
    endif()

    find_library(
        HERMES_HAIKU_WEBKIT_LEGACY_LIBRARY
        NAMES WebKitLegacy libWebKitLegacy
        HINTS ${_HERMES_HAIKU_WEBKIT_LIBRARY_ROOT_HINTS}
        NO_DEFAULT_PATH
    )

    if(HERMES_HAIKU_WEBKIT_INCLUDE_ROOT
       AND HERMES_HAIKU_WEBKIT_API_INCLUDE_DIR
       AND HERMES_HAIKU_WEBKIT_LEGACY_LIBRARY)
        add_library(hermes_haiku_webkit_legacy INTERFACE)
        add_library(hermes::haiku_webkit_legacy ALIAS hermes_haiku_webkit_legacy)
        target_include_directories(
            hermes_haiku_webkit_legacy
            INTERFACE
                "${HERMES_HAIKU_WEBKIT_INCLUDE_ROOT}"
                "${HERMES_HAIKU_WEBKIT_API_INCLUDE_DIR}"
        )
        target_link_libraries(
            hermes_haiku_webkit_legacy
            INTERFACE
                "${HERMES_HAIKU_WEBKIT_LEGACY_LIBRARY}"
        )
        set(HERMES_HAS_HAIKU_WEBKIT 1)
    else()
        string(JOIN ", " _HERMES_HAIKU_WEBKIT_HEADER_ROOTS ${_HERMES_HAIKU_WEBKIT_HEADER_ROOT_HINTS})
        string(JOIN ", " _HERMES_HAIKU_WEBKIT_LIBRARY_ROOTS ${_HERMES_HAIKU_WEBKIT_LIBRARY_ROOT_HINTS})
        message(FATAL_ERROR
            "HERMES_BUILD_HAIKU_SHELL requires the Haiku WebKitLegacy system packages "
            "(haikuwebkit and haikuwebkit_devel). "
            "Hemera probes for <WebView.h> plus JavaScriptCore/JavaScript.h and libWebKitLegacy. "
            "Header roots checked: ${_HERMES_HAIKU_WEBKIT_HEADER_ROOTS}. "
            "Library roots checked: ${_HERMES_HAIKU_WEBKIT_LIBRARY_ROOTS}. "
            "Override with HERMES_HAIKU_WEBKIT_INCLUDE_ROOT and/or HERMES_HAIKU_WEBKIT_LIBRARY_ROOT if your install lives elsewhere.")
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
