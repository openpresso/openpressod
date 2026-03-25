add_library(state_manager_objs OBJECT)

target_sources(state_manager_objs PRIVATE
    state_manager.hpp
    state_manager.cpp
)

target_link_libraries(state_manager_objs PUBLIC
    libopenpresso::libopenpresso
    openpresso_proto::openpresso_proto
    config_objs
)

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)
target_include_directories(state_manager_objs INTERFACE ${PARENT_DIR})
