includes("math/xmake.lua")
includes("interop/xmake.lua")

target("engine")
    set_kind("static")
    add_deps("hitagi_interop", {public = true})
    add_files("**/*.cppm", {public = true})
    add_files("**/*.cpp", {public = true})
    remove_files("interop/*.cppm")
    remove_files("interop/*.cpp")
    remove_files("editor/*.cppm")
    remove_files("editor/*.cpp")
    remove_files("editor/**/*.cpp")
    remove_files("test/*.cppm")
    remove_files("test/*.cpp")
    remove_files("*/test/*.cpp")
    remove_files("math/ispc/*.cpp")
    set_options("ispc")
    if has_config("ispc") then
        add_deps("ispc_math")
    end

    if is_plat("windows") then
        add_syslinks("Ole32", "winmm", "imm32", "User32", {public = true})
    else
        remove_files("gfx/dx12/*.cppm")
        remove_files("gfx/dx12/*.cpp")
    end

    if is_plat("linux") then
        add_syslinks("pthread", {public = true})
    end

target("unit_tests")
    set_default(false)
    set_group("test")
    set_rundir("$(projectdir)")
    add_deps("engine", "hitagi_interop_test")
    add_includedirs("test")
    add_files("test/test_utils.cppm")
    add_files("test/unit_test_main.cpp")
    add_files("*/test/*.cpp")
    remove_files("*/test/*_test_main.cpp")
    remove_files("*/test/*_benchmark.cpp")
    on_run(function (target)
        import("xmake.scripts.run_unit_tests", {rootdir = os.projectdir()}).main(target)
    end)

if has_config("benchmarks") then
    for name, source in pairs({memory_benchmark = "core/test/memory_benchmark.cpp", ecs_benchmark = "ecs/test/ecs_benchmark.cpp"}) do
        target(name)
            set_kind("binary")
            set_default(false)
            set_group("benchmark")
            set_rundir("$(projectdir)")
            add_deps("engine", "hitagi_interop_test", "hitagi_interop_benchmark")
            add_includedirs("test")
            add_files("test/test_utils.cppm", source)
    end
end

includes("editor/xmake.lua")
