target("render_graph_test")
    add_files("render_graph_test.cpp")
    add_deps("graphics", "resource", "utils", "test_utils")