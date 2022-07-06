option("ispc")
    set_description("Enable or disable ispc for building math library")
    set_default(true)
    set_showmenu(true)
    add_defines("USE_ISPC")


target("math")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_packages("fmt", {public = true})
    set_options("ispc", {public = true})
    if has_config("ispc") then 
        add_deps("ispc_math", {public = true})
    end