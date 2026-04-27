target("engine")
    set_kind("static")
    add_files("*.cppm", {public = true})
    add_files("*.cpp")
    add_deps(
        "core",
        "math",
        "hid",
        "ecs",
        "gfx",
        "asset",
        "debugger",
        "gui",
        "app",
        "render"
    )
