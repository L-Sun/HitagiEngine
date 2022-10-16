set_project("HitagiEngine")
set_languages("c++20")

includes("xmake-rules/*.lua")
add_rules("mode.debug", "mode.release", "mode.releasedbg", "clang-msvc", "copy-dll")

if is_mode("debug") then
    add_defines("_DEBUG")
end
set_runtimes(is_mode("debug") and "MTd" or "MT")

add_requireconfs("*", {configs = {shared = true}})
add_requires("taskflow", "cxxopts", "nlohmann_json", "yaml-cpp", "directxshadercompiler")

includes("hitagi/**/xmake.lua")
includes("examples/**/xmake.lua")
includes("tools/xmake.lua")
