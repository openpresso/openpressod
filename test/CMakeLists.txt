find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(openpressod_config_test 
    openpressod_config_test.cpp
)

target_link_libraries(openpressod_config_test PRIVATE config_objs GTest::gtest_main)

gtest_add_tests(TARGET openpressod_config_test)