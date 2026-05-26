cmake_minimum_required(VERSION 3.20)

project(HermesHemera LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HERMES_BUILD_HAIKU_SHELL_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    set(HERMES_BUILD_HAIKU_SHELL_DEFAULT ON)
endif()

option(HERMES_BUILD_HAIKU_SHELL "Build the native Haiku shell." ${HERMES_BUILD_HAIKU_SHELL_DEFAULT})
option(HERMES_ENABLE_TESTS "Build the Haiku port tests." ON)
option(HERMES_ENABLE_NATIVE_PAIGE "Enable native Paige runtime integration." OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(HermesDependencies)

add_subdirectory(src)

if(HERMES_ENABLE_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
