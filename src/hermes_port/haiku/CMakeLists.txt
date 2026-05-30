file(READ "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/app/hemera-mail.hvif" HEMERA_APP_ICON_HEX HEX)

add_executable(
    Hemera
    HaikuMessageWindow.cpp
    HaikuFindSupport.cpp
    HaikuComposeWindow.cpp
    HaikuIconCatalog.cpp
    HaikuToolbarSupport.cpp
    HaikuWebKitSupport.cpp
    Main.cpp
    HaikuMainWindow.cpp
    HaikuShellHost.cpp
    HaikuWazooWindow.cpp
    PaigeEditorView.cpp
)

target_link_libraries(
    Hemera
    PRIVATE
        hermes::port_core
        hermes::port_paige
        hermes::legacy_transplants
        hermes::haiku_webkit_legacy
)

target_include_directories(
    Hemera
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(
    Hemera
    PRIVATE
        HERMES_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

if(HERMES_IS_HAIKU)
    find_program(HEMERA_RC_EXECUTABLE rc)
    find_program(HEMERA_XRES_EXECUTABLE xres)
    find_program(HEMERA_PACKAGE_EXECUTABLE package)

    set(HEMERA_APP_DIRECTORY "apps/Hemera")
    set(HEMERA_PACKAGEINFO_PATH "${CMAKE_CURRENT_BINARY_DIR}/.PackageInfo")
    set(HEMERA_RDEF_PATH "${CMAKE_CURRENT_BINARY_DIR}/Hemera.rdef")
    set(HEMERA_RSRC_PATH "${CMAKE_CURRENT_BINARY_DIR}/Hemera.rsrc")
    set(HEMERA_STAGE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/hemera-package-root")
    set(HEMERA_HPKG_PATH
        "${CMAKE_CURRENT_BINARY_DIR}/hemera-${HEMERA_HAIKU_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.hpkg")

    string(REGEX REPLACE "([0-9A-Fa-f]{64})" "\\1\"\n    $\"" HEMERA_APP_ICON_HEX_RDEF "${HEMERA_APP_ICON_HEX}")

    configure_file("${CMAKE_SOURCE_DIR}/packaging/haiku/Hemera.rdef.in" "${HEMERA_RDEF_PATH}" @ONLY)
    configure_file("${CMAKE_SOURCE_DIR}/packaging/haiku/Hemera.PackageInfo.in" "${HEMERA_PACKAGEINFO_PATH}" @ONLY)

    target_link_libraries(
        Hemera
        PRIVATE
            be
    )

    if(HEMERA_RC_EXECUTABLE AND HEMERA_XRES_EXECUTABLE)
        add_custom_command(
            TARGET Hemera
            POST_BUILD
            COMMAND "${HEMERA_RC_EXECUTABLE}" -o "${HEMERA_RSRC_PATH}" "${HEMERA_RDEF_PATH}"
            COMMAND "${HEMERA_XRES_EXECUTABLE}" -o "$<TARGET_FILE:Hemera>" "${HEMERA_RSRC_PATH}"
            VERBATIM
        )
    endif()

    if(HEMERA_PACKAGE_EXECUTABLE)
        add_custom_command(
            OUTPUT "${HEMERA_HPKG_PATH}"
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${HEMERA_STAGE_ROOT}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${HEMERA_STAGE_ROOT}/${HEMERA_APP_DIRECTORY}/icons"
            COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:Hemera>"
                    "${HEMERA_STAGE_ROOT}/${HEMERA_APP_DIRECTORY}/Hemera"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/toolbar"
                    "${HEMERA_STAGE_ROOT}/${HEMERA_APP_DIRECTORY}/icons/toolbar"
            COMMAND "${CMAKE_COMMAND}" -E copy "${HEMERA_PACKAGEINFO_PATH}" "${HEMERA_STAGE_ROOT}/.PackageInfo"
            COMMAND "${HEMERA_PACKAGE_EXECUTABLE}" create -C "${HEMERA_STAGE_ROOT}" "${HEMERA_HPKG_PATH}"
            DEPENDS Hemera
            VERBATIM
        )
        add_custom_target(hemera_hpkg ALL DEPENDS "${HEMERA_HPKG_PATH}")
    endif()
endif()

install(TARGETS Hemera DESTINATION apps/Hemera)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/toolbar" DESTINATION apps/Hemera/icons)
