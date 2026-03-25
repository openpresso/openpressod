target_sources(openpressod PRIVATE
    main.cpp
)

add_subdirectory(config)
add_subdirectory(state_manager)
target_link_libraries(openpressod PRIVATE 
    config_objs 
    state_manager_objs
)


