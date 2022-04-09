target("utils")
    set_kind("headeronly")
    add_includedirs("include", {public = true})

target("utils_test")
    set_kind("headeronly")
    add_headerfiles("utils/test.hpp")
    add_includedirs("include", {public = true})
    add_deps("math", {public = true})
    add_packages("gtest", "gmock", "benchmark", "spdlog", {public = true})