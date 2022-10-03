target("dx12")
    -- TODO try use dll
    set_kind("static")
    add_files("src/dx12/*.cpp")
    add_includedirs("include")
    add_deps("core", "math", "utils")
    add_syslinks("d3d12", "dxgi", "dxguid")
    add_packages("directxshadercompiler")
    add_defines("NOMINMAX")

    if not is_plat("windows") then 
        set_enabled(false)
    end

option("graphics_api")
    if is_plat("windows") then
        set_default("dx12")
    end
    set_showmenu(true)
    set_values("dx12")

target("gfx")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/*.cpp")
    add_deps("core", "math", "$(graphics_api)", {public = true})