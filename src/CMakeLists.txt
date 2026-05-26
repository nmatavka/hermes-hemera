add_subdirectory(hermes_port/core)

if(HERMES_BUILD_HAIKU_SHELL)
    add_subdirectory(hermes_port/haiku)
endif()
