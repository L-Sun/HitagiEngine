# ===============================================================
# Math Test
# ===============================================================
add_executable(math_test math_test.cpp)
target_link_libraries(math_test PRIVATE math test_utils)
add_test(
    NAME TEST_math
    COMMAND math_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
