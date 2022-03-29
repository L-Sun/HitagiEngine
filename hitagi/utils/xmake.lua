target("test_utils")
    set_kind("phony")
    add_includedirs("include", {interface = true})
    add_packages("gtest", "gmock", "benchmark", {interface = true})