function(openpressod_targets_compile_warnings)
    foreach(TARGET_NAME IN LISTS ARGN)
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
endfunction()

function(declare_clang_tools_targets_lists)
    set_property(GLOBAL PROPERTY TIDY_TARGETS)
    set_property(GLOBAL PROPERTY FORMAT_TARGETS)
endfunction()

function(append_targets_to_clang_tidy_list)
    set_property(GLOBAL APPEND PROPERTY TIDY_TARGETS ${ARGN})
endfunction()

function(append_targets_to_clang_format_list)
    set_property(GLOBAL APPEND PROPERTY FORMAT_TARGETS ${ARGN})
endfunction()

function(process_clang_tools_targets)
    get_property(TIDY_TARGETS_LIST GLOBAL PROPERTY TIDY_TARGETS)
    get_property(FORMAT_TARGETS_LIST GLOBAL PROPERTY FORMAT_TARGETS)

    list(APPEND FORMAT_TARGETS_LIST ${TIDY_TARGETS_LIST})

    add_clang_format_targets(clang_format_validate TARGETS ${FORMAT_TARGETS_LIST} DRY_RUN)
    add_clang_format_targets(clang_format_apply TARGETS ${FORMAT_TARGETS_LIST})

    add_clang_tidy_targets(clang_tidy_validate TARGETS ${TIDY_TARGETS_LIST} ARGS --quiet -p=${CMAKE_BINARY_DIR} --warnings-as-errors=*)
    add_clang_tidy_targets(clang_tidy_analyze TARGETS ${TIDY_TARGETS_LIST} ARGS --quiet -p=${CMAKE_BINARY_DIR})
endfunction()

macro(create_deb_package)
    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/openpressod.service" 
        "${CMAKE_BINARY_DIR}/packaging/openpressod.service"
        @ONLY
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/postinst" 
        "${CMAKE_BINARY_DIR}/packaging/postinst"
        @ONLY
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                                GROUP_READ GROUP_EXECUTE
                                WORLD_READ WORLD_EXECUTE
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/prerm" 
        "${CMAKE_BINARY_DIR}/packaging/prerm"
        @ONLY
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                                GROUP_READ GROUP_EXECUTE
                                WORLD_READ WORLD_EXECUTE
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/postrm" 
        "${CMAKE_BINARY_DIR}/packaging/postrm"
        @ONLY
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                                GROUP_READ GROUP_EXECUTE
                                WORLD_READ WORLD_EXECUTE
    )

    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/conffiles" 
        "${CMAKE_BINARY_DIR}/packaging/conffiles"
        @ONLY
    )

    set(OPENPRESSOD_TARGET ${CMAKE_SYSTEM_PROCESSOR} CACHE STRING "")

    install(TARGETS openpressod
        RUNTIME DESTINATION "${OPENPRESSOD_BIN_DIR}"
        COMPONENT openpressod-deb-package-content
        EXCLUDE_FROM_ALL
    )

    install(FILES "${CMAKE_BINARY_DIR}/packaging/openpressod.service"
        DESTINATION "lib/systemd/system"
        COMPONENT openpressod-deb-package-content
        EXCLUDE_FROM_ALL
    )

    install(FILES "${CMAKE_SOURCE_DIR}/packaging/${OPENPRESSOD_CONFIG_EXAMPLE_NAME}"
        DESTINATION "${OPENPRESSOD_CONFIG_DIR}"
        COMPONENT openpressod-deb-package-content
        EXCLUDE_FROM_ALL
    )

    install(FILES "${CMAKE_SOURCE_DIR}/packaging/60-openpressod.rules"
        DESTINATION "lib/udev/rules.d"
        COMPONENT openpressod-deb-package-content
        EXCLUDE_FROM_ALL
    )

    set(PACKAGE_FILE_NAME "openpressod_${OPENPRESSOD_VERSION}_${OPENPRESSOD_TARGET}")
    message(STATUS "${PACKAGE_FILE_NAME}")

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
    set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${CMAKE_PROJECT_NAME};openpressod-deb-package-content;/")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_BINARY_DIR}/packaging/postinst"
        "${CMAKE_BINARY_DIR}/packaging/prerm"
        "${CMAKE_BINARY_DIR}/packaging/postrm"
        "${CMAKE_BINARY_DIR}/packaging/conffiles"
    )

    include(CPack)

    install(FILES "${CMAKE_BINARY_DIR}/${PACKAGE_FILE_NAME}.deb"
        DESTINATION .
        COMPONENT installer
        EXCLUDE_FROM_ALL
    )
endmacro()