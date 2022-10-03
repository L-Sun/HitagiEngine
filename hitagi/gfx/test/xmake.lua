target("device_test")
    add_files("device_test.cpp")
    add_deps("application", "gfx", "utils", "test_utils")
    set_group("test/gfx")
