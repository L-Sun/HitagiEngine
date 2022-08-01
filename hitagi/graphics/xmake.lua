option("graphics_api")
    if is_plat("windows") then
        set_default("dx12")
    end
    set_showmenu(true)
    set_values("dx12")

target("graphics_dx12")
    -- TODO try use dll
    set_kind("static")
    add_files("src/backend/dx12/*.cpp")
    add_includedirs("include")
    add_deps("core", "math", "resource", "utils")
    add_syslinks("d3d12", "dxgi", "dxguid", "dxcompiler")

target("graphics")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "math", "resource", "application", "utils")
    if is_config("graphics_api", "dx12") then
        add_deps("graphics_dx12")
    end