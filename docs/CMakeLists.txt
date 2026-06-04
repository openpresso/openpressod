FetchContent_Declare(doxygen-awesome-css
    GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css
    GIT_TAG v2.4.2)
FetchContent_MakeAvailable(doxygen-awesome-css)
FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)

set(DOXY_PAGES
    index.md
    installation.md
    config.md
    usage.md
)

foreach(DOCFILE ${DOXY_PAGES})
  configure_file(pages/${DOCFILE} ${CMAKE_CURRENT_BINARY_DIR}/${DOCFILE} @ONLY)
endforeach()

list(TRANSFORM DOXY_PAGES PREPEND "${CMAKE_CURRENT_BINARY_DIR}/")
list(JOIN DOXY_PAGES " " DOXY_PAGES_STR)
  
set(DOXYGEN_INPUT "${DOXY_PAGES_STR}")
set(DOXYGEN_HTML_DIR ${CMAKE_CURRENT_BINARY_DIR}/html)
set(DOXYGEN_LATEX_DIR ${CMAKE_CURRENT_BINARY_DIR}/pdf)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
configure_file(Doxyfile.in ${DOXYFILE_OUT} @ONLY)

add_doxygen_target(doxy
    DOXYFILE ${DOXYFILE_OUT} 
    DEPENDS 
        ${DOXY_PAGES} 
        ${CMAKE_CURRENT_SOURCE_DIR}/header.html 
        ${CMAKE_CURRENT_SOURCE_DIR}/custom.css
        ${CMAKE_CURRENT_SOURCE_DIR}/logo-light.svg
        ${CMAKE_CURRENT_SOURCE_DIR}/logo-dark.svg
)

set(MAN_DIR ${CMAKE_CURRENT_BINARY_DIR}/man)
set(MAN_PAGES
    openpressod.conf.5.md
    openpressod.8.md
)

foreach(man_page ${MAN_PAGES})
  configure_file(pages/${man_page} ${CMAKE_CURRENT_BINARY_DIR}/${man_page} @ONLY)
endforeach()

list(TRANSFORM MAN_PAGES PREPEND "${CMAKE_CURRENT_BINARY_DIR}/")

add_pandoc_man_target(man
    ALL
    INPUTS ${MAN_PAGES}
    OUTPUT_DIR ${MAN_DIR}
)

add_latex_target(pdf
    OUTPUT_NAME openpressod
    WORKDIR "${DOXYGEN_LATEX_DIR}"
)

add_dependencies(pdf doxy)

add_custom_target(docs)

add_dependencies(docs pdf doxy)

install(DIRECTORY "${DOXYGEN_HTML_DIR}/"
  DESTINATION docs
  COMPONENT DocumentationHtml
  EXCLUDE_FROM_ALL)

install(FILES "${DOXYGEN_LATEX_DIR}/openpressod.pdf"
  DESTINATION .
  COMPONENT DocumentationPdf
  EXCLUDE_FROM_ALL)

install(DIRECTORY "${MAN_DIR}/"
  DESTINATION usr/share/man
  COMPONENT openpressod-deb-package-content
  EXCLUDE_FROM_ALL)

