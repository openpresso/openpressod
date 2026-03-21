add_library(config_objs OBJECT)

target_sources(config_objs PRIVATE
    openpressod_config.hpp
    openpressod_config.cpp
    config_defaults.hpp
    libopenpresso_config_constants.hpp
    libopenpresso_config_labels.hpp
    libopenpresso_config_maker.cpp
    libopenpresso_config_maker.hpp
)

target_link_libraries(config_objs PUBLIC
    toml11::toml11 
    libopenpresso::libopenpresso
    magic_enum::magic_enum
)

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)
target_include_directories(config_objs INTERFACE ${PARENT_DIR})
