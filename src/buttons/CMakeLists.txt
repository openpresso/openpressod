add_library(buttons_objs OBJECT)

target_sources(buttons_objs PRIVATE
    buttons_event_sink.hpp
    buttons_callback_registrator.hpp
    buttons_callback_registrator.cpp
)

target_link_libraries(buttons_objs PUBLIC
    libopenpresso::libopenpresso
    config_objs
)

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)
target_include_directories(buttons_objs INTERFACE ${PARENT_DIR})
