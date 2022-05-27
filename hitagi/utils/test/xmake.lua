target("soa_test")
    add_files("soa_test.cpp")
    add_deps("utils", "test_utils")
    set_group("test/utils")