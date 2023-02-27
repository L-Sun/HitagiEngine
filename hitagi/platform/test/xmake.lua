target("app_test")
    add_files("*.cpp")
    add_deps("app", "utils", "test_utils")