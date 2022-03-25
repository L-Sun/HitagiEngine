# ===============================================================
# File IO Test
# ===============================================================
add_executable(
    file_io_manager_test
    file_io_manager_test.cpp
)
target_link_libraries(
    file_io_manager_test
    PRIVATE file_io_manager
)
add_test(
    NAME TEST_file_io_manager
    COMMAND file_io_manager_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# ===============================================================
# Timer Test
# ===============================================================
add_executable(
    timer_test
    timer_test.cpp
)
target_link_libraries(
    timer_test
    PRIVATE timer GTest::gtest
)
add_test(
    NAME TEST_timer_test
    COMMAND timer_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# ===============================================================
# Thread Test
# ===============================================================
add_executable(
    thread_pool_test
    thread_pool_test.cpp
)
target_link_libraries(
    thread_pool_test
    PRIVATE thread_manager
)
add_test(
    NAME TEST_thread_pool_test
    COMMAND thread_pool_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
