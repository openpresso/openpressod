add_executable(brew_log_test 
    brew_log_manager_test.cpp 
    brew_log_sampler_test.cpp

)

target_link_libraries(brew_log_test 
    PRIVATE 
        openpressod_objects 
        libopenpresso_mock 
        GTest::gtest_main
)

openpressod_targets_compile_warnings(brew_log_test)
gtest_add_tests(TARGET brew_log_test TEST_LIST brew_log_tests)

set_tests_properties(${brew_log_tests} PROPERTIES TIMEOUT 15)

append_targets_to_clang_format_list(brew_log_test)
