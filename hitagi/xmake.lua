includes("utils/xmake.lua")
includes("math/xmake.lua")
includes("core/xmake.lua")
includes("gfx/xmake.lua")
includes("hid/xmake.lua")
includes("ecs/xmake.lua")
includes("platform/xmake.lua")
includes("debugger/xmake.lua")
includes("gui/xmake.lua")
includes("asset/xmake.lua")
includes("physics/xmake.lua")
includes("render/xmake.lua")
includes("test_utils/xmake.lua")
includes("engine/xmake.lua")

includes("utils/test/xmake.lua")
includes("core/test/xmake.lua")
includes("math/test/xmake.lua")
includes("ecs/test/xmake.lua")
includes("gfx/test/xmake.lua")
includes("platform/test/xmake.lua")
includes("gui/test/xmake.lua")
includes("asset/test/xmake.lua")
includes("render/test/xmake.lua")

local test_groups = {
    tests_utils = {"soa_test"},
    tests_core = {"memory_test", "memory_benchmark", "file_io_manager_test", "timer_test"},
    tests_math = {"math_test"},
    tests_ecs = {"ecs_test", "ecs_benchmark"},
    tests_gfx = {"gfx_test", "shader_compiler_test", "device_test", "render_graph_test"},
    tests_app = {"app_test"},
    tests_gui = {"gui_test"},
    tests_asset = {"mesh_test", "material_test", "codec_test", "transform_test"},
    tests_render = {"renderer_test"},
}

for group, deps in pairs(test_groups) do
    target(group)
        set_kind("phony")
        set_default(false)
        set_group("test")
        for _, dep in ipairs(deps) do
            add_deps(dep)
        end
end

target("tests")
    set_kind("phony")
    set_default(false)
    set_group("test")
    for group, _ in pairs(test_groups) do
        add_deps(group)
    end
