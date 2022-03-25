# ===============================================================
# Memory Test
# ===============================================================
add_executable(memory_test memory_test.cpp)
target_link_libraries(memory_test PRIVATE memory_manager test_utils)
gtest_discover_tests(memory_test WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(memory_benchmark memory_benchmark.cpp)
target_link_libraries(memory_benchmark PRIVATE memory_manager test_utils)

# ===============================================================
# File IO Test
# ===============================================================
add_executable(file_io_manager_test file_io_manager_test.cpp)
target_link_libraries(file_io_manager_test PRIVATE file_io_manager test_utils)
gtest_discover_tests(
    file_io_manager_test WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# ===============================================================
# Timer Test
# ===============================================================
add_executable(timer_test timer_test.cpp)
target_link_libraries(timer_test PRIVATE timer test_utils)
gtest_discover_tests(timer_test WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
