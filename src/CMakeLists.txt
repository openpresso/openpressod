target_sources(openpressod PRIVATE
    main.cpp
)

add_subdirectory(config)
target_link_libraries(openpressod PRIVATE config_objs)
