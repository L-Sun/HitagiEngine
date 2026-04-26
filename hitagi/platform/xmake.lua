target("app")
    set_kind("phony")
    set_default(false)
    set_group("modules")
    add_files("app.cppm", "app_base.cppm", {public = true})
    add_files("app_impl.cpp")
    add_deps("core", "hid")
    add_packages("nlohmann_json", {public = true})

    if is_plat("windows") then
        add_files("app_windows.cppm", {public = true})
        add_syslinks("winmm", "imm32", "User32", {public = true})
        add_defines("UNICODE", "WIN32", {public = true})
    end

    add_files("app_sdl3.cppm", {public = true})
    add_packages("libsdl3", {public = true})
