target("core")
    set_kind("phony")
    set_default(false)
    set_group("modules")
    add_files("core.cppm", {public = true})
    add_files("*.cpp")
    add_deps("utils")
    add_packages("tracy", {public = true})
    if is_os("linux") then
        add_syslinks("pthread", {public = true})
    end
