target("utils")
    set_kind("headeronly")
    add_includedirs("include", {public = true})

target("test_utils")
    set_kind("phony")
    add_includedirs("include", {interface = true})
    add_packages("gtest", "gmock", "benchmark", "spdlog", {interface = true})