find_program(LATEXMK_EXECUTABLE NAMES latexmk)
if(LATEXMK_EXECUTABLE)
    execute_process(COMMAND "${LATEXMK_EXECUTABLE}" --version 
        OUTPUT_VARIABLE LATEXMK_VERSION)
    string(STRIP "${LATEXMK_VERSION}" LATEXMK_VERSION)
    message(STATUS "${LATEXMK_VERSION}")
else()
    message(WARNING "latexmk not found, PDF will be skipped.")
endif()

set(SPHINX_CONFIGURED_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/sphinx_sources")

configure_file(
    conf.py
    "${SPHINX_CONFIGURED_SOURCE_DIR}/conf.py"
    @ONLY
)

file(COPY pages/ DESTINATION "${SPHINX_CONFIGURED_SOURCE_DIR}")

add_sphinx_docs(html_docs 
    FORMAT html 
    SOURCE_DIR "${SPHINX_CONFIGURED_SOURCE_DIR}" 
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/html_docs"
)

add_sphinx_docs(latex_docs 
    FORMAT latex 
    SOURCE_DIR "${SPHINX_CONFIGURED_SOURCE_DIR}" 
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/pdf_docs"
)

if(LATEXMK_EXECUTABLE)
    add_custom_target(pdf_docs
        COMMAND "${LATEXMK_EXECUTABLE}" -pdf -interaction=nonstopmode
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdf_docs"
        DEPENDS latex_docs
    )
endif()

add_sphinx_docs(man_docs
    ALL
    FORMAT man 
    SOURCE_DIR "${SPHINX_CONFIGURED_SOURCE_DIR}" 
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/man_docs"
)