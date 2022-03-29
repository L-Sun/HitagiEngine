target("memory_test")
    add_files("memory_test.cpp")
    add_deps("memory_manager", "test_utils")
    add_packages("fmt")

target("file_io_manager_test")
    add_files("file_io_manager_test.cpp")
    add_deps("file_io_manager", "test_utils")

target("timer_test")
    add_files("timer_test.cpp")
    add_deps("timer", "test_utils")