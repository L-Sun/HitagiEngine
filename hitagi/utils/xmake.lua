target("utils")
    set_kind("static")
    add_files("*.cppm", {public = true})
    add_files("*.cpp")
    add_packages("magic_enum", "spdlog", {public = true})
    if is_plat("windows") then
        add_syslinks("Ole32", {public = true})
    end

includes("test")