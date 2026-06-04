find_package(ClangFormat QUIET)

if(NOT CLANG_FORMAT_FOUND)
    find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
    if(CLANG_FORMAT_EXECUTABLE)
        set(CLANG_FORMAT_FOUND TRUE)
    endif()
endif()

if(CLANG_FORMAT_FOUND)
    execute_process(COMMAND "${CLANG_FORMAT_EXECUTABLE}" --version 
        OUTPUT_VARIABLE CLANG_FORMAT_VERSION)
    string(REGEX MATCH "[^\n]*version[^\n]*" CLANG_FORMAT_VERSION "${CLANG_FORMAT_VERSION}")
    string(STRIP "${CLANG_FORMAT_VERSION}" CLANG_FORMAT_VERSION)
    message(STATUS "Clang-format: ${CLANG_FORMAT_VERSION}")
endif()

function(add_clang_format_target CUSTOM_TARGET_NAME)
    if(NOT CLANG_FORMAT_FOUND)
        message(WARNING "clang-format not found, skipping format target creation for ${CUSTOM_TARGET_NAME}")
        return()
    endif()

    set(options DRY_RUN APPEND)
    set(oneValueArgs TARGET)
    set(multiValueArgs)
    cmake_parse_arguments(FORMAT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT FORMAT_TARGET)
        message(FATAL_ERROR "TARGET argument is required for add_clang_format_target")
    endif()

    # Get all sources and headers from the target
    get_target_property(TARGET_SOURCES ${FORMAT_TARGET} SOURCES)
    get_target_property(TARGET_SOURCES_DIR ${FORMAT_TARGET} SOURCE_DIR)
    get_target_property(HEADER_SETS ${FORMAT_TARGET} HEADER_SETS)
    get_target_property(INTERFACE_HEADER_SETS ${FORMAT_TARGET} INTERFACE_HEADER_SETS)

    set(ALL_FILES "")
    foreach(source_file ${TARGET_SOURCES})
        if(source_file MATCHES "-NOTFOUND")
            continue()
        endif()
        if(NOT IS_ABSOLUTE "${source_file}")
            string(PREPEND source_file "${TARGET_SOURCES_DIR}/")
        endif()
        list(APPEND ALL_FILES "${source_file}")
    endforeach()

    foreach(SET_NAME ${HEADER_SETS} ${INTERFACE_HEADER_SETS})
        if(SET_NAME MATCHES "-NOTFOUND")
            continue()
        endif()
        get_target_property(SET_FILES ${FORMAT_TARGET} HEADER_SET_${SET_NAME})
        if(SET_FILES)
            list(APPEND ALL_FILES ${SET_FILES})
        endif()
    endforeach()

    if(NOT ALL_FILES)
        message(WARNING "No sources or header sets found for target ${FORMAT_TARGET}")
        return()
    endif()

    list(REMOVE_DUPLICATES ALL_FILES)

    set(FORMAT_COMMAND ${CLANG_FORMAT_EXECUTABLE})
    if(FORMAT_DRY_RUN)
        list(APPEND FORMAT_COMMAND --dry-run --Werror)
    else()
        list(APPEND FORMAT_COMMAND -i)
    endif()

    if(NOT TARGET ${CUSTOM_TARGET_NAME})
        file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/run_${CUSTOM_TARGET_NAME}.cmake"
            CONTENT "\
                set(FILES \$<FILTER:$<TARGET_PROPERTY:${CUSTOM_TARGET_NAME},SOURCES>,EXCLUDE,CMakeFiles/>)
                if(FILES)
                    execute_process(COMMAND ${FORMAT_COMMAND} \${FILES} COMMAND_ERROR_IS_FATAL ANY)
                endif()
            ")

        add_custom_target(${CUSTOM_TARGET_NAME}
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/run_${CUSTOM_TARGET_NAME}.cmake"
            COMMENT "Formatting ${CUSTOM_TARGET_NAME} with clang-format"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            VERBATIM
        )
    endif()

    if(ALL_FILES)
        target_sources(${CUSTOM_TARGET_NAME} PRIVATE ${ALL_FILES})
    endif()
    
endfunction()

function(add_clang_format_targets CUSTOM_TARGET_NAME)
    if(NOT CLANG_FORMAT_FOUND)
        message(WARNING "clang-format not found, skipping format target creation for ${CUSTOM_TARGET_NAME}")
        return()
    endif()

    set(options DRY_RUN)
    set(oneValueArgs)
    set(multiValueArgs TARGETS)
    cmake_parse_arguments(FORMAT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT FORMAT_TARGETS)
        message(WARNING "No targets specified for clang-format")
        return()
    endif()

    list(REMOVE_DUPLICATES FORMAT_TARGETS)

    foreach(target ${FORMAT_TARGETS})
        set(target_args "")
        if(FORMAT_DRY_RUN)
            list(APPEND target_args DRY_RUN)
        endif()
        add_clang_format_target(${CUSTOM_TARGET_NAME} APPEND TARGET ${target} ${target_args})
    endforeach()
endfunction()
