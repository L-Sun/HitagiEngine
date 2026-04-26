includes("base/xmake.lua")
includes("mock/xmake.lua")
includes("dx12/xmake.lua")
includes("vulkan/xmake.lua")
includes("render_graph/xmake.lua")

target("gfx")
    set_kind("phony")
    set_default(false)
    set_group("modules")
    set_basename("hitagi_gfx")
    add_files("gfx.cppm", {public = true})
    add_files("gfx.cpp", {public = true})
    add_deps(
        "gfx_base",
        "gfx_mock",
        "gfx_vulkan",
        "gfx_render_graph"
    )

    if is_plat("windows") then
        add_deps("gfx_dx12")
    end
