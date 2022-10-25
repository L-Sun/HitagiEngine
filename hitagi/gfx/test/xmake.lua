target("device_test")
    add_files("device_test.cpp")
    add_deps("app", "gfx", "utils", "test_utils")
    set_group("test/gfx")

target("render_graph_test")
    add_files("render_graph_test.cpp")
    add_deps("app", "gfx", "utils", "test_utils")
    set_group("test/gfx")
