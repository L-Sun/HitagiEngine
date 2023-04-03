add_requires("slang", "directx12-agility-sdk")

target("gfx_test")
    add_files("gfx_test.cpp")
    add_deps("gfx", "utils", "test_utils")
    set_group("test/gfx")

target("shader_compiler_test")
    add_files("shader_compiler_test.cpp")
    add_deps("utils", "test_utils")
    add_deps("shader_compiler")
    set_group("test/gfx")

target("device_test")
    add_files("device_test.cpp")
    add_deps("app", "utils", "test_utils")
    add_deps("gfx_device")
    set_group("test/gfx")

target("render_graph_test")
    add_files("render_graph_test.cpp")
    add_deps("app", "gfx", "utils", "test_utils")
    set_group("test/gfx")