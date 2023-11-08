target("render")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_deps("gfx", "asset", "gui")

includes("test")