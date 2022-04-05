set_project("HitagiEngine")
set_languages("c++20")

if is_mode("debug") then
    set_symbols("debug")
end

add_requires("spdlog", "fmt", "magic_enum", "gtest", "benchmark")

includes("hitagi/**/xmake.lua")