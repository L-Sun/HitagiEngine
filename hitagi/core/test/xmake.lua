target("memory_test")
    add_files("memory_test.cpp")
    add_deps("memory_manager", "test_utils")
    add_packages("fmt")

