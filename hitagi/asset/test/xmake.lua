target("mesh_test")
    add_files("mesh_test.cpp")
    add_deps("asset", "test_utils")
    set_group("test/asset")
    
target("material_test")
    add_files("material_test.cpp")
    add_deps("asset", "test_utils")
    set_group("test/asset")


target("codec_test")
    add_files("codec_test.cpp")
    add_deps("asset", "core", "test_utils")
    set_group("test/asset")

target("transform_test")
    add_files("transform_test.cpp")
    add_deps("asset", "test_utils")
    set_group("test/transform")