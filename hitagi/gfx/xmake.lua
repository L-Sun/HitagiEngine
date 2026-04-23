includes("base/xmake.lua")
includes("mock/xmake.lua")
includes("dx12/xmake.lua")
includes("vulkan/xmake.lua")
includes("render_graph/xmake.lua")
includes("test")

target("gfx")
    set_kind("static")
    set_basename("hitagi_gfx")
    add_files("gfx.cppm", "gfx.cpp", {public = true})
    add_deps(
        "gfx_base",
        "gfx_mock",
        "gfx_vulkan",
        "gfx_render_graph"
    )

    if is_plat("windows") then
        add_deps("gfx_dx12")
    end
