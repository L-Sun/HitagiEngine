# ===============================================================
# Math Test
# ===============================================================
add_executable(math_test math_test.cpp)
target_link_libraries(math_test PRIVATE math test_utils)
gtest_discover_tests(math_test WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
