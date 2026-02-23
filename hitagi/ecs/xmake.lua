target("ecs")
    set_kind("static")
    add_files("*.cppm", {public = true})
    add_files("*.cpp")
    add_deps("core")
    add_packages("taskflow", {public = true})

includes("test")
