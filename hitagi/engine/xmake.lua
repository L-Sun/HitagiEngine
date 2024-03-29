target("engine")
    set_kind("static")
    add_files("src/engine.cpp")
    add_includedirs("include", {public = true})
    add_deps(
        "core",
        "math",
        "hid",
        "ecs",
        "gfx",
        "asset",
        "debugger",
        "asset_manager",
        "gui",
        "app",
        "render",
        {public = true}
    )