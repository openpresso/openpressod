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