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
    add_packages("fmt", "magic_enum", {public = true})

target("test_utils")
    set_kind("headeronly")
    add_headerfiles("utils/test.hpp")
    add_includedirs("include", {public = true})
    add_deps("math", {public = true})
    add_packages("gtest", "gmock", "benchmark", "spdlog", {public = true})