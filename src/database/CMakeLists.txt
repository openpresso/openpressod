target_sources(openpressod_objects PRIVATE
    database_id.hpp
    sqlite_enum.hpp
    brew_profile_fingerprint.hpp
    steps_schema.hpp
)

add_subdirectory(brew_profiles)
add_subdirectory(brew_history)
