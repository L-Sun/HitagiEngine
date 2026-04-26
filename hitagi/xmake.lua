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
    tests_utils = {"utils_tests"},
    tests_core = {"core_tests"},
    tests_math = {"math_tests"},
    tests_ecs = {"ecs_tests"},
    tests_gfx = {"gfx_tests"},
    tests_app = {"app_tests"},
    tests_gui = {"gui_tests"},
    tests_asset = {"asset_tests"},
    tests_render = {"render_tests"},
}

local benchmark_groups = {
    benchmarks_core = {"core_benchmarks"},
    benchmarks_ecs = {"ecs_benchmarks"},
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

for group, deps in pairs(benchmark_groups) do
    target(group)
        set_kind("phony")
        set_default(false)
        set_group("benchmark")
        for _, dep in ipairs(deps) do
            add_deps(dep)
        end
end

target("benchmarks")
    set_kind("phony")
    set_default(false)
    set_group("benchmark")
    for group, _ in pairs(benchmark_groups) do
        add_deps(group)
    end
