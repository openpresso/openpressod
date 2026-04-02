target_sources(openpressod PRIVATE
    main.cpp
)

add_subdirectory(config)
add_subdirectory(buttons)
add_subdirectory(state_manager)
target_link_libraries(openpressod PRIVATE 
    config_objs
    buttons_objs
    state_manager_objs
)


