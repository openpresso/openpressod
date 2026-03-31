find_package(ClangTidy QUIET)

if(NOT CLANG_TIDY_FOUND)
    find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
    if(CLANG_TIDY_EXECUTABLE)
        set(CLANG_TIDY_FOUND TRUE)
    endif()
endif()

if(CLANG_TIDY_FOUND)
    execute_process(COMMAND "${CLANG_TIDY_EXECUTABLE}" --version 
        OUTPUT_VARIABLE CLANG_TIDY_VERSION)
    string(REGEX MATCH "[^\n]*version[^\n]*" CLANG_TIDY_VERSION "${CLANG_TIDY_VERSION}")
    string(STRIP "${CLANG_TIDY_VERSION}" CLANG_TIDY_VERSION)
    message(STATUS "Clang-tidy: ${CLANG_TIDY_VERSION}")
endif()

function(add_clang_tidy_target CUSTOM_TARGET_NAME)
    if(NOT CLANG_TIDY_FOUND)
        message(WARNING "clang-tidy not found, skipping tidy target creation for ${CUSTOM_TARGET_NAME}")
        return()
    endif()

    set(options APPEND)
    set(oneValueArgs TARGET)
    set(multiValueArgs ARGS)
    cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT arg_TARGET)
        message(FATAL_ERROR "TARGET argument is required for add_clang_tidy_target")
    endif()

    # Get all sources and headers from the target
    get_target_property(TARGET_SOURCES ${arg_TARGET} SOURCES)
    get_target_property(TARGET_SOURCES_DIR ${arg_TARGET} SOURCE_DIR)
    get_target_property(HEADER_SETS ${arg_TARGET} HEADER_SETS)
    get_target_property(INTERFACE_HEADER_SETS ${arg_TARGET} INTERFACE_HEADER_SETS)

    set(ALL_FILES "")
    foreach(source_file ${TARGET_SOURCES})
        if(NOT IS_ABSOLUTE "${source_file}")
            string(PREPEND source_file "${TARGET_SOURCES_DIR}/")
        endif()
        list(APPEND ALL_FILES "${source_file}")
    endforeach()

    foreach(SET_NAME ${HEADER_SETS} ${INTERFACE_HEADER_SETS})
        if(SET_NAME MATCHES "-NOTFOUND")
            continue()
        endif()
        get_target_property(SET_FILES ${arg_TARGET} HEADER_SET_${SET_NAME})
        if(SET_FILES)
            list(APPEND ALL_FILES ${SET_FILES})
        endif()
    endforeach()

    if(NOT ALL_FILES)
        message(WARNING "No sources or header sets found for target ${arg_TARGET}")
        return()
    endif()

    list(REMOVE_DUPLICATES ALL_FILES)
    
    if(NOT TARGET ${CUSTOM_TARGET_NAME} OR NOT arg_APPEND)
        add_custom_target(${CUSTOM_TARGET_NAME})
    endif()

    foreach(tidy_file ${ALL_FILES})
        cmake_path(HASH tidy_file tidy_file_path_hash)
        cmake_path(GET tidy_file FILENAME tidy_file_name)
        set(tidy_target_name "${tidy_file_name}-${CUSTOM_TARGET_NAME}.${tidy_file_path_hash}")

        add_custom_target(${tidy_target_name}
            COMMAND ${CLANG_TIDY_EXECUTABLE} ${arg_ARGS} ${tidy_file}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

        set_target_properties(${tidy_target_name} PROPERTIES FOLDER ${CUSTOM_TARGET_NAME})
        add_dependencies(${CUSTOM_TARGET_NAME} ${tidy_target_name})
    endforeach()
endfunction()

function(add_clang_tidy_targets CUSTOM_TARGET_NAME)
    if(NOT CLANG_TIDY_FOUND)
        message(WARNING "clang-tidy not found, skipping tidy target creation for ${CUSTOM_TARGET_NAME}")
        return()
    endif()

    set(options "")
    set(oneValueArgs)
    set(multiValueArgs TARGETS ARGS)
    cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT arg_TARGETS)
        message(FATAL_ERROR "TARGETS argument is required for add_clang_tidy_targets")
    endif()

    foreach(target ${arg_TARGETS})
        add_clang_tidy_target(${CUSTOM_TARGET_NAME} APPEND TARGET ${target} ARGS ${arg_ARGS})
    endforeach()
endfunction()
