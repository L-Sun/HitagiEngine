add_requires("gtest", "benchmark")

target("test_utils")
    set_kind("phony")
    add_includedirs("include", {interface = true})
    add_packages("gtest", "benchmark", {interface = true})