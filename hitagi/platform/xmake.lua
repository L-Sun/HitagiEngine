add_requires("libsdl")

target("app")
    set_kind("static")
    add_files("src/application.cpp")
    add_includedirs("include", {public = true})
    add_deps("core", "hid")
    add_packages("nlohmann_json")

    if is_plat("windows") then
        add_files("src/windows/*.cpp")
        add_syslinks("winmm", "imm32", "User32")
        add_defines("UNICODE", "WIN32")
    end

    add_files("src/sdl2/*.cpp")
    add_packages("libsdl")
