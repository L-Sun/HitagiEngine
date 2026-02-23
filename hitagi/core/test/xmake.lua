target("memory_test")
    add_files("memory_test.cpp")
    add_deps("core", "test_utils")
    set_group("test/core")

target("memory_benchmark")
    add_files("memory_benchmark.cpp")
    add_deps("core", "test_utils")
    set_group("test/core")

target("file_io_manager_test")
    add_files("file_io_manager_test.cpp")
    add_deps("core", "test_utils")
    set_group("test/core")

target("timer_test")
    add_files("timer_test.cpp")
    add_deps("core", "test_utils")
    set_group("test/core")
