option("ispc")
    set_description("Enable or disable ispc for building math library")
    set_default(true)
    set_showmenu(true)

target("math")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_packages("fmt", {public = true})
    if has_config("ispc") then 
        add_deps("ispc_math", {public = true})
        add_defines("USE_ISPC", {public = true})
    end
