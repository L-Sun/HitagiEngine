set_project("HitagiEngine")
set_languages("c++20")

if is_mode("debug") then
    set_symbols("debug")
end


add_requires("fmt", "magic_enum", "gtest", "benchmark")
add_requires("spdlog", {configs = {fmt_external = true}})

includes("hitagi/**/xmake.lua")