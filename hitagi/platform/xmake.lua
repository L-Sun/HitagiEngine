target("application")
    set_kind("static")
    add_files("src/application.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "hid")

    if is_plat("windows") then
        add_files("src/windows/*.cpp")
        add_syslinks("winmm", "imm32")
        add_defines("UNICODE", "WIN32")
    end