target("hid")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "math", "utils")
    add_packages("magic_enum", "spdlog")

