target("graphics_dx12")
    -- TODO try use dll
    set_kind("static")
    add_files("src/backend/dx12/*.cpp")
    add_includedirs("include")
    add_deps("core", "math", "enum_types")

target("graphics")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "math", "enum_types")