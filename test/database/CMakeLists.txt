add_executable(database_test
    brew_profiles_repository_test.cpp
    brew_history_repository_test.cpp
)

target_link_libraries(database_test PRIVATE
    openpressod_objects
    GTest::gtest_main
)

openpressod_targets_compile_warnings(database_test)
gtest_add_tests(TARGET database_test TEST_LIST database_tests)
append_targets_to_clang_format_list(database_test)
