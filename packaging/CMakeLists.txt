set(OPENPRESSOD_CONFIG_EXAMPLE_PATH "${OPENPRESSOD_CONFIG_DIR}/openpressod.conf.example")
set(OPENPRESSOD_SERVICE_NAME openpressod)
set(OPENPRESSOD_SYSTEM_USER openpressod)
set(OPENPRESSOD_BIN_DIR /usr/bin)

configure_file(
    openpressod.service
    "${CMAKE_CURRENT_BINARY_DIR}/openpressod.service"
    @ONLY
)

configure_file(
    conffiles
    "${CMAKE_CURRENT_BINARY_DIR}/conffiles"
    @ONLY
)

configure_file(
    postinst
    "${CMAKE_CURRENT_BINARY_DIR}/postinst"
    @ONLY
    FILE_PERMISSIONS 
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
    )

configure_file(
    prerm
    "${CMAKE_CURRENT_BINARY_DIR}/prerm"
    @ONLY
    FILE_PERMISSIONS 
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

configure_file(
    postrm 
    "${CMAKE_CURRENT_BINARY_DIR}/postrm"
    @ONLY
    FILE_PERMISSIONS 
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

set(OPENPRESSOD_TARGET ${CMAKE_SYSTEM_PROCESSOR} CACHE STRING "")

install(TARGETS openpressod
    RUNTIME DESTINATION "${OPENPRESSOD_BIN_DIR}"
    COMPONENT openpressod-deb-package-content
    EXCLUDE_FROM_ALL
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/openpressod.service"
    DESTINATION "lib/systemd/system"
    COMPONENT openpressod-deb-package-content
    EXCLUDE_FROM_ALL
)

install(FILES "openpressod.conf.example"
    DESTINATION "${OPENPRESSOD_CONFIG_DIR}"
    COMPONENT openpressod-deb-package-content
    EXCLUDE_FROM_ALL
)

install(FILES "60-openpressod.rules"
    DESTINATION "lib/udev/rules.d"
    COMPONENT openpressod-deb-package-content
    EXCLUDE_FROM_ALL
)

set(PACKAGE_FILE_NAME "openpressod_${OPENPRESSOD_VERSION}_${OPENPRESSOD_TARGET}")

# --- CPack ---
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "openpressod")
set(CPACK_DEBIAN_FILE_NAME "${PACKAGE_FILE_NAME}")
set(CPACK_PACKAGE_VERSION "${OPENPRESSOD_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenPresso daemon")
set(CPACK_PACKAGE_CONTACT "Openpresso <openpresso@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "misc")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6, libgcc-s1")
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${CMAKE_PROJECT_NAME};openpressod-deb-package-content;/")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_BINARY_DIR}/postinst"
    "${CMAKE_CURRENT_BINARY_DIR}/prerm"
    "${CMAKE_CURRENT_BINARY_DIR}/postrm"
    "${CMAKE_CURRENT_BINARY_DIR}/conffiles"
)

include(CPack)

install(FILES "${PROJECT_BINARY_DIR}/${PACKAGE_FILE_NAME}.deb"
    DESTINATION .
    COMPONENT installer
    EXCLUDE_FROM_ALL
)