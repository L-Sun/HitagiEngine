set_project("HitagiEngine")
set_languages("c++20")

includes("xmake-rules/*.lua")
add_rules("mode.debug", "mode.release", "clang-msvc")

add_requires("magic_enum", "gtest", "benchmark", "taskflow", "cxxopts", "yaml-cpp", "imgui")
add_requires("fmt", {configs = {header_only = true}})
add_requires("spdlog", {configs = {fmt_external = true}})

includes("hitagi/**/xmake.lua")
includes("tools/xmake.lua")
