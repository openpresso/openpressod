find_package(GTest REQUIRED)
include(GoogleTest)

add_subdirectory(mock)

add_subdirectory(config)
add_subdirectory(state_manager)
add_subdirectory(brew_history)

add_subdirectory(database)
add_subdirectory(service)
