target("mesh_test")
    add_files("mesh_test.cpp")
    add_deps("mesh", "test_utils")
    add_packages("spdlog")

target("parser_test")
    add_files("parser_test.cpp")
    add_deps("parser", "asset_manager", "test_utils")