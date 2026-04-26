target("app")
    set_kind("phony")
    set_default(false)
    set_group("modules")
    add_files("app.cppm", {public = true})
    add_files("app_impl.cpp")
    add_deps("core", "hid")
    add_packages("nlohmann_json", "libsdl3", {public = true})

    if is_plat("windows") then
        add_syslinks("winmm", "imm32", "User32", {public = true})
        add_defines("UNICODE", "WIN32", {public = true})
    end