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

add_requires(
    "taskflow",
    "cxxopts",
    "nlohmann_json",
    "range-v3",
    "vulkansdk",
    "vulkan-memory-allocator",
    "directx-shader-compiler",
    "spirv-reflect"
)

-- Only compiled libraries that we intentionally link dynamically need this setting.
add_requires(
    "libpng",
    "libjpeg-turbo",
    "freetype",
    "libsdl3",
    "joltphysics",
    "gtest",
    {configs = {shared = true}}
)

-- Match spdlog's external fmt dependency instead of mixing header-only and shared builds.
add_requires("fmt", {configs = {header_only = true, shared = false}})
add_requires("magic_enum", {configs = {modules = true}})
add_requires("tracy v0.13.1", {configs = {shared = true}})
add_requires("spdlog", {configs = {fmt_external = true}})
add_requireconfs("spdlog.fmt", {version = "latest", override = true, configs = {header_only = true, shared = false}})
add_requires("imgui v1.92.9+b-docking", {configs = {shared = true, freetype = true, wchar32 = true}})
if is_plat("windows") then
    add_requires("d3d12-memory-allocator", {configs = {shared = true}})
    add_requires("directx12-agility-sdk")
end
if has_config("benchmarks") then
    add_requires("benchmark", {configs = {shared = true}})
end

includes("hitagi/xmake.lua")
includes("examples/**/xmake.lua")
