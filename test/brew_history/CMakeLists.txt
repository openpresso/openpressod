add_executable(brew_history_test 
    brew_history_manager_test.cpp 
    brew_history_sampler_test.cpp

)

target_link_libraries(brew_history_test 
    PRIVATE 
        openpressod_objects 
        libopenpresso_mock 
        GTest::gtest_main
)

openpressod_targets_compile_warnings(brew_history_test)
gtest_add_tests(TARGET brew_history_test TEST_LIST brew_history_tests)

set_tests_properties(${brew_history_tests} PROPERTIES TIMEOUT 15)

append_targets_to_clang_format_list(brew_history_test)
