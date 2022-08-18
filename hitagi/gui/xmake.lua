target("gui")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "hid", "graphics", "application")
    add_packages("imgui", {public = true})