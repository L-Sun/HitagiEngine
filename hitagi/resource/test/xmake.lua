target("mesh_test")
    add_files("mesh_test.cpp")
    add_deps("mesh", "utils_test")
    set_group("test/resource")

target("material_test")
    add_files("material_test.cpp")
    add_deps("material", "utils_test")
    set_group("test/resource")


target("parser_test")
    add_files("parser_test.cpp")
    add_deps("parser", "asset_manager", "utils_test")
    set_group("test/resource")