add_requires("imgui v1.89-docking", {configs = {freetype = true, wchar32 = true}})

target("gui")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "hid", "gfx", "app")
    add_packages("imgui", {public = true})