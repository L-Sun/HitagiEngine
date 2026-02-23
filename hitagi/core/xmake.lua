target("core")
    set_kind("static")
    add_files("*.cppm", {public = true})
    add_files("*.cpp")
    add_deps("utils")
    add_packages("tracy", {public = true})
    if is_os("linux") then
        add_syslinks("pthread")
    end

includes("test")
