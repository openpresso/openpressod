target_sources(openpressod_objects PRIVATE
    sqlite_enum.hpp
    brew_profile_fingerprint.hpp
    steps_schema.hpp
)

add_subdirectory(brew_profiles)
