set_project("HitagiEngine")
set_languages("c++23")

add_repositories("local-repo xmake")

includes("xmake/rules/*.lua")
add_rules(
    "mode.debug",
    "mode.release",
    "mode.releasedbg",
    "inject_env"
)

if is_mode("debug") then
    add_defines("HITAGI_DEBUG")
end

if is_plat("windows") then 
    set_encodings("utf-8")
    if is_mode("debug") then
        set_runtimes("MDd")
    else 
        set_runtimes("MD")
    end
end

option("profile")
    set_default(false)
    set_description("Enable tracy profiling.")
option_end()

option("examples")
    set_default(false)
    set_description("Include game and GUI examples in the default build.")
option_end()

option("benchmarks")
    set_default(false)
    set_description("Enable benchmark dependencies and targets.")
option_end()

if has_config("profile") then
    add_defines("TRACY_ENABLE")
    if is_plat("windows") then
        add_defines("TRACY_IMPORTS")
    end
end

includes("hitagi/xmake.lua")
includes("examples/**/xmake.lua")
