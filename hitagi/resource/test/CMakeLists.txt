# ===============================================================
# Parser Test
# ===============================================================
add_executable(parser_test parser_test.cpp)
target_link_libraries(parser_test PRIVATE asset_manager test_utils)
gtest_discover_tests(parser_test WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
