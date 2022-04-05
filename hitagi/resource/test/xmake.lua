target("mesh_test")
    add_files("mesh_test.cpp")
    add_deps("mesh", "test_utils")
    add_packages("spdlog")