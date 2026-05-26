add_subdirectory(hermes_port/core)
add_subdirectory(hermes_port/paige)
add_subdirectory(legacy_transplants)

if(HERMES_BUILD_HAIKU_SHELL)
    add_subdirectory(hermes_port/haiku)
endif()
