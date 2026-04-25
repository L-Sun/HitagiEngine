target("render")
    set_kind("static")
    add_files("*.cppm", {public = true})
    add_files("*.cpp")
    add_files("text/*.cpp")
    add_deps("gfx", "asset", "gui")
    add_packages("range-v3", "freetype")

includes("test")
