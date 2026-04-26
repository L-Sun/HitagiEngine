target("utils")
    set_kind("phony")
    set_default(false)
    set_group("modules")
    add_files("utils.cppm", {public = true})
    add_files("*.cpp", {public = true})
    add_packages("magic_enum", "spdlog", {public = true})
    if is_plat("windows") then
        add_syslinks("Ole32", {public = true})
    end
