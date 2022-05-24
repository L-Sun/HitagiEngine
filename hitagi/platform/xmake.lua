target("application")
    set_kind("headeronly")
    add_headerfiles("include/hitagi/application.hpp")
    add_includedirs("include", {public = true})
    add_deps("runtime_module_interface")

target("engine")
    set_kind("static")
    add_files("src/application.cpp")
    if is_plat("windows") then
        add_files("src/windows/*.cpp")
        add_syslinks("winmm")
        add_defines("UNICODE")
    end
    add_deps(
        "core",
        "math",
        "hid",
        "ecs",
        "graphics",
        "resource",
        "debugger",
        "asset_manager",
        "scene_manager",
        "gui",
        "gameplay",
        {public = true}
    )