set_project("HitagiEngine")
set_languages("c++20")
set_symbols("debug")

add_requires("spdlog", "fmt", "gtest", "benchmark")

includes("hitagi/**/xmake.lua")