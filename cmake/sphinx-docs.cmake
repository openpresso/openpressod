if(NOT SPHINX_FOUND)
    find_program(SPHINX_EXECUTABLE NAMES sphinx-build)
    if(SPHINX_EXECUTABLE)
        set(SPHINX_FOUND TRUE)
    endif()
endif()

if(SPHINX_FOUND)
    execute_process(COMMAND "${SPHINX_EXECUTABLE}" --version 
        OUTPUT_VARIABLE SPHINX_VERSION)
    string(STRIP "${SPHINX_VERSION}" SPHINX_VERSION)
    message(STATUS "${SPHINX_VERSION}")
endif()

function(add_sphinx_docs TARGET_NAME)
    if(NOT SPHINX_FOUND)
        message(WARNING "sphinx not found, docs target ${TARGET_NAME} will be skipped")
        return()
    endif()

    set(_opts   ALL)
    set(_single FORMAT SOURCE_DIR OUTPUT_DIR)
    cmake_parse_arguments(SPHINX_ARG "${_opts}" "${_single}" "" ${ARGN})

    if(NOT SPHINX_ARG_FORMAT)
        message(FATAL_ERROR "add_sphinx_docs(${TARGET_NAME}): FORMAT is required")
    endif()
    if(NOT SPHINX_ARG_SOURCE_DIR)
        message(FATAL_ERROR "add_sphinx_docs(${TARGET_NAME}): SOURCE_DIR is required")
    endif()
    if(NOT SPHINX_ARG_OUTPUT_DIR)
        message(FATAL_ERROR "add_sphinx_docs(${TARGET_NAME}): OUTPUT_DIR is required")
    endif()

    if(NOT IS_ABSOLUTE "${SPHINX_ARG_SOURCE_DIR}")
        set(SPHINX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${SPHINX_ARG_SOURCE_DIR}")
    else()
        set(SPHINX_SOURCE_DIR "${SPHINX_ARG_SOURCE_DIR}")
    endif()

    if(NOT IS_ABSOLUTE "${SPHINX_ARG_OUTPUT_DIR}")
        set(SPHINX_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${SPHINX_ARG_OUTPUT_DIR}")
    else()
        set(SPHINX_OUTPUT_DIR "${SPHINX_ARG_OUTPUT_DIR}")
    endif()

    set(SPHINX_DOCTREES_DIR "${CMAKE_CURRENT_BINARY_DIR}/.doctrees_${TARGET_NAME}")

    set(_all "")
    if(ARG_ALL)
        set(_all ALL)
    endif()

    add_custom_target("${TARGET_NAME}" ${_all}
        COMMAND "${SPHINX_EXECUTABLE}"
                -j auto
                -b "${SPHINX_ARG_FORMAT}"
                -d "${SPHINX_DOCTREES_DIR}"
                "${SPHINX_SOURCE_DIR}"
                "${SPHINX_OUTPUT_DIR}"
        VERBATIM
    )
endfunction()