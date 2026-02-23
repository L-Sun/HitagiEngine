target("app")
    set_kind("static")
    add_files("app.cppm", "app_base.cppm", {public = true})
    add_files("app_impl.cpp")
    add_deps("core", "hid")
    add_packages("nlohmann_json")

    if is_plat("windows") then
        add_files("app_windows.cppm")
        add_syslinks("winmm", "imm32", "User32")
        add_defines("UNICODE", "WIN32")
    end

    add_files("app_sdl3.cppm")
    add_packages("libsdl3")

includes("test")
