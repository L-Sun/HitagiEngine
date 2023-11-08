add_requires("magic_enum", "gtest", "benchmark")
add_requires("fmt", "spdlog")

target("utils")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/*.cpp")
    remove_files("src/test.cpp")
    add_packages("fmt", "magic_enum", "spdlog", "range-v3", {public = true})


target("test_utils")
    set_kind("static")
    add_files("src/test.cpp")
    add_includedirs("include", {public = true})
    add_deps("math", {public = true})
    add_packages("gtest", "gmock", "benchmark", "spdlog", {public = true})