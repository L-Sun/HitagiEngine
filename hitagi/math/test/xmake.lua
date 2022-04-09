target("math_test")
    add_files("math_test.cpp")
    add_deps("math", "utils_test")
    set_group("test/math")
