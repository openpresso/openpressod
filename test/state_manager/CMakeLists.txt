add_executable(status_manager_test status_manager_test.cpp)
target_link_libraries(status_manager_test PRIVATE
    openpressod_objects
    libopenpresso_mock
    GTest::gtest_main
)
gtest_add_tests(TARGET status_manager_test)

append_targets_to_clang_format_list(status_manager_test)
