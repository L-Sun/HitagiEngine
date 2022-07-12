set_project("HitagiEngine")
set_languages("c++20")

includes("xmake-rules/*.lua")
add_rules("mode.debug", "mode.release", "clang-msvc", "copy-dll")

if is_mode("debug") then
    add_defines("_DEBUG")
end
set_runtimes(is_mode("debug") and "MTd" or "MT")

add_requireconfs("*", {configs = {shared = true}})
add_requires("magic_enum", "crossguid", "gtest", "benchmark", "taskflow", "cxxopts", "nlohmann_json", "yaml-cpp")
add_requires("imgui", {configs = {freetype = true, wchar32 = true}})
add_requires("fmt", {configs = {header_only = true}})
add_requires("spdlog", {configs = {fmt_external = true}})

includes("hitagi/**/xmake.lua")
includes("examples/**/xmake.lua")
includes("tools/xmake.lua")
