find_program(doxygen_EXECUTABLE doxygen)
if(doxygen_EXECUTABLE)
    add_executable(doxygen::doxygen IMPORTED GLOBAL)
    set_property(TARGET doxygen::doxygen PROPERTY IMPORTED_LOCATION ${doxygen_EXECUTABLE})

    execute_process(COMMAND "${doxygen_EXECUTABLE}" --version OUTPUT_VARIABLE DOXYGEN_VERSION)
    string(STRIP "${DOXYGEN_VERSION}" DOXYGEN_VERSION)
    message(STATUS "Doxygen: ${DOXYGEN_VERSION}")
else()
    message(WARNING "doxygen not found")
endif()

find_program(latexmk_EXECUTABLE latexmk)
if(latexmk_EXECUTABLE)
    add_executable(latexmk::latexmk IMPORTED GLOBAL)
    set_property(TARGET latexmk::latexmk PROPERTY IMPORTED_LOCATION ${latexmk_EXECUTABLE})

    execute_process(COMMAND "${latexmk_EXECUTABLE}" --version OUTPUT_VARIABLE LATEXMK_VERSION)
    string(STRIP "${LATEXMK_VERSION}" LATEXMK_VERSION)
    message(STATUS "${LATEXMK_VERSION}")
else()
    message(WARNING "latexmk not found")
endif()

function(add_doxygen_target target_name)
    if(NOT TARGET doxygen::doxygen)
        message(WARNING "Doxygen not found, target ${target_name} will not be created")
        return()
    endif()

    set(options ALL)
    set(one_value_args DOXYFILE)
    set(multi_value_args DEPENDS)

    cmake_parse_arguments(DOXYGEN_ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT DOXYGEN_ARG_DOXYFILE)
        message(FATAL_ERROR "add_doxygen_target: DOXYFILE is required")
        return()
    endif()

    if(NOT IS_ABSOLUTE "${DOXYGEN_ARG_DOXYFILE}")
        set(DOXYFILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${DOXYGEN_ARG_DOXYFILE}")
    else()
        set(DOXYFILE_PATH "${DOXYGEN_ARG_DOXYFILE}")
    endif()

    set(TARGET_KEYWORDS ${target_name})
    if(DOXYGEN_ARG_ALL)
        list(APPEND TARGET_KEYWORDS ALL)
    endif()

    set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${target_name}.stamp")

    if(DOXYGEN_ARG_DEPENDS)
        add_custom_command(
            OUTPUT ${STAMP_FILE}
            COMMAND doxygen::doxygen "${DOXYFILE_PATH}"
            COMMAND ${CMAKE_COMMAND} -E touch "${STAMP_FILE}"
            DEPENDS ${DOXYFILE_PATH} ${DOXYGEN_ARG_DEPENDS}
        )

        add_custom_target(${TARGET_KEYWORDS} 
            DEPENDS ${STAMP_FILE})
    else()
        add_custom_target(${TARGET_KEYWORDS}
            COMMAND doxygen::doxygen "${DOXYFILE_PATH}"
            COMMAND ${CMAKE_COMMAND} -E touch "${STAMP_FILE}"
        )
    endif()
    
    set_target_properties(${target_name} PROPERTIES STAMP_FILE "${STAMP_FILE}")
endfunction()

function(add_latex_target target_name)
    if(NOT TARGET latexmk::latexmk)
        message(WARNING "Latexmk not found, target ${target_name} will not be created")
        return()
    endif()

    set(options ALL)
    set(one_value_args OUTPUT_NAME WORKDIR)
    set(multi_value_args "")

    cmake_parse_arguments(LATEXMK_ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT LATEXMK_ARG_WORKDIR)
        message(FATAL_ERROR "add_latex_target: WORKDIR is required")
        return()
    endif()

    if(LATEXMK_ARG_OUTPUT_NAME)
        set(jobname_arg "-jobname=${LATEXMK_ARG_OUTPUT_NAME}")
    endif()

    set(TARGET_KEYWORDS ${target_name})
    if(LATEXMK_ARG_ALL)
        list(APPEND TARGET_KEYWORDS ALL)
    endif()

    add_custom_target(${TARGET_KEYWORDS}
        COMMAND latexmk::latexmk -pdf -interaction=nonstopmode ${jobname_arg} refman
        WORKING_DIRECTORY "${DOXYGEN_LATEX_DIR}"
    )
endfunction()