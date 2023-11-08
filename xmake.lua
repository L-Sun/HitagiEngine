set_project("HitagiEngine")
set_languages("c++20")

add_repositories("local-repo xmake")

includes("xmake/rules/*.lua")
add_rules(
    "mode.debug",
    "mode.release",
    "mode.releasedbg",
    "clang-msvc",
    "copy-dll",
    "inject_env"
)

if is_mode("debug") then
    add_defines("HITAGI_DEBUG", "_DEBUG")
end

if is_plat("windows") then 
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

if has_config("profile") then 
    add_defines("TRACY_ENABLE")
    if is_plat("windows") then
        add_defines("TRACY_IMPORTS")
    end
end

add_requireconfs("*", {configs = {shared = true}})
add_requires("taskflow", "cxxopts", "nlohmann_json", "tracy", "range-v3")

includes("hitagi/**/xmake.lua")
includes("examples/**/xmake.lua")
-- includes("tools/xmake.lua")