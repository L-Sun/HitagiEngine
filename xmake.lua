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
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})


if is_mode("debug") then
    add_defines("HITAGI_DEBUG", "_DEBUG")
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

if has_config("profile") then
    add_defines("TRACY_ENABLE")
    add_requireconfs("tracy", {configs = {on_demand = true}})
    if is_plat("windows") then
        add_defines("TRACY_IMPORTS")
    end
end

add_requireconfs("*", {configs = {shared = true}})

add_requires(
    "taskflow",
    "cxxopts",
    "nlohmann_json",
    "tracy",
    "range-v3",
    "vulkansdk",
    "vulkan-memory-allocator",
    "directx-shader-compiler",
    "spirv-reflect",
    "libpng",
    "assimp",
    "libjpeg-turbo",
    "fx-gltf",
    "libsdl2",
    "magic_enum",
    "gtest",
    "benchmark",
    "fmt"
)

add_requires("spdlog", {configs = {fmt_external = true}})
add_requires("imgui v1.92.1-docking", {configs = {freetype = true, wchar32 = true}})
add_requires("d3d12-memory-allocator", "directx12-agility-sdk", {optional = true})

includes("hitagi/**/xmake.lua")
includes("examples/**/xmake.lua")