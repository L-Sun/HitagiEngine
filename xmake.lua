set_project("HitagiEngine")
set_languages("c++20")
add_rules("mode.debug", "mode.release")


add_requires("fmt", "magic_enum", "gtest", "benchmark")
add_requires("spdlog", {configs = {fmt_external = true}})

includes("hitagi/**/xmake.lua")