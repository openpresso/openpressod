add_executable(brew_profiles_repository_test
    brew_profiles_repository_test.cpp
)

target_link_libraries(brew_profiles_repository_test PRIVATE
    openpressod_objects
    GTest::gtest_main
)

openpressod_targets_compile_warnings(brew_profiles_repository_test)
gtest_add_tests(TARGET brew_profiles_repository_test)
append_targets_to_clang_format_list(brew_profiles_repository_test)
