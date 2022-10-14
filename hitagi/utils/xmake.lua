add_requires("magic_enum", "crossguid", "gtest", "benchmark")
add_requires("fmt", {configs = {header_only = true}})
add_requires("spdlog")

target("utils")
    set_kind("headeronly")
    add_headerfiles(
        "include/hitagi/utils/concepts.hpp",
        "include/hitagi/utils/hash.hpp",
        "include/hitagi/utils/soa.hpp",
        "include/hitagi/utils/overloaded.hpp",
        "include/hitagi/utils/private_build.hpp"
    )
    add_includedirs("include", {public = true})
    add_packages("fmt", "magic_enum", "spdlog", {public = true})


target("test_utils")
    set_kind("static")
    add_files("src/test.cpp")
    add_includedirs("include", {public = true})
    add_deps("math", {public = true})
    add_packages("gtest", "gmock", "benchmark", "spdlog", {public = true})